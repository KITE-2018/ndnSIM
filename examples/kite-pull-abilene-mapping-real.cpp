/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2017 Harbin Institute of Technology, China
 *
 * Author: Zhongda Xia <xiazhongda@hit.edu.cn>
 **/

// ndn-simple-kite.cpp

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"

#include "ns3/point-to-point-module.h"

#include "ns3/wifi-net-device.h"
#include "ns3/sta-wifi-mac.h"

#include "ns3/ndnSIM-module.h"

#include "apps/kite-ms-server.hpp"
#include "apps/kite-ms-mobile.hpp"
#include "apps/kite-ms-consumer.hpp"

#include "ns3/ndnSIM/NFD/daemon/fw/trace-forwarding.hpp"

#include <boost/algorithm/string.hpp>

NS_LOG_COMPONENT_DEFINE("kite.pullmap");

namespace ns3 {

void
getNodeInfo(Ptr<Node> pNode)
{
  Ptr<ndn::L3Protocol> pP = pNode->GetObject<ndn::L3Protocol>();
  for (int i = 0; i < pNode->GetNDevices(); i++) {
    auto nd = pNode->GetDevice(i);
    NS_LOG_INFO("NetDevice: " << nd);
    auto ch = nd->GetChannel();
    for (uint32_t deviceId = 0; deviceId < ch->GetNDevices(); deviceId++) {
      Ptr<NetDevice> otherSide = ch->GetDevice(deviceId);
      NS_LOG_INFO("Otherside: " << otherSide);
    }
    NS_LOG_INFO(pP->getFaceByNetDevice(nd)->getId());
    NS_LOG_INFO(pP->getFaceByNetDevice(nd)->getLocalUri());
    NS_LOG_INFO(pP->getFaceByNetDevice(nd)->getRemoteUri());
  }

  const nfd::Fib& fib = pP->getForwarder()->getFib();
  for (auto entry = fib.begin(); entry != fib.end(); entry++) {
    NS_LOG_INFO(entry->getPrefix());
    auto nextHops = entry->getNextHops();
    for (auto nextHop = nextHops.begin(); nextHop != nextHops.end(); nextHop++) {
      NS_LOG_INFO(nextHop->getFace().getId());
    }
  }
}

void
StaAssoc(string context, Mac48Address maddr)
{
  vector<string> tokens;
  boost::split(tokens, context, boost::is_any_of("/"));
  int nodeId = atoi(tokens[2].c_str());
  int deviceId = atoi(tokens[4].c_str());

  Ptr<Node> node = NodeList::GetNode(nodeId);

  Ptr<ns3::WifiNetDevice> wifiDev = node->GetDevice(deviceId)->GetObject<ns3::WifiNetDevice>();
  if (wifiDev != nullptr) {
    Ptr<ns3::StaWifiMac> staMac = wifiDev->GetMac()->GetObject<ns3::StaWifiMac>();
    if (staMac != nullptr) {
      Address dest = staMac->GetBssid();
      Ssid ssid = staMac->GetSsid();
      NS_LOG_INFO("STA Associated: Node "
                  << nodeId << ", Device " << deviceId << ", MAC: " << staMac->GetAddress()
                  << ", to MAC: " << maddr << ", Bssid: " << dest << ", Ssid: " << ssid);

      int i, j;
      Ptr<Node> pNode, pTargetNode;
      pTargetNode = NULL;
      for (i = 0; i < NodeList::GetNNodes(); i++) {
        pNode = NodeList::GetNode(i);
        for (j = 0; j < pNode->GetNDevices(); j++) {
          if (Mac48Address::ConvertFrom(pNode->GetDevice(j)->GetAddress()) == maddr) {
            pTargetNode = pNode;
            i = NodeList::GetNNodes();
            break;
          }
        }
      }
      if (pTargetNode != NULL) {
        NS_LOG_INFO("Node found: " << pTargetNode->GetId());
        pTargetNode->GetObject<ndn::L3Protocol>()->getForwarder()->m_gone = false;
        // Ptr<Node> mobile = NodeList::GetNode(12);
        // ndn::KiteMsServer* app = dynamic_cast<ndn::KiteMsServer*>(&(*server->GetApplication(0)));
        std::string prefix = "/router/" + std::to_string(pTargetNode->GetId());
        // app->SetAttribute("Locator", StringValue(prefix));
        ndn::KiteMsMobile* app = dynamic_cast<ndn::KiteMsMobile*>(&(*node->GetApplication(0)));
        app->m_locator = prefix;
        Simulator::Schedule(Seconds(0.01), &ndn::KiteMsMobile::UpdateMapping, app);
      }
      else {
        NS_LOG_INFO("Node not found.");
      }
    }
  }
}

void
StaDeAssoc(string context, Mac48Address maddr)
{
  // purge stale states on deassociation
  vector<string> tokens;
  boost::split(tokens, context, boost::is_any_of("/"));
  int nodeId = atoi(tokens[2].c_str());
  int deviceId = atoi(tokens[4].c_str());

  Ptr<Node> node = NodeList::GetNode(nodeId);

  Ptr<ns3::WifiNetDevice> wifiDev = node->GetDevice(deviceId)->GetObject<ns3::WifiNetDevice>();
  if (wifiDev != nullptr) {
    Ptr<ns3::StaWifiMac> staMac = wifiDev->GetMac()->GetObject<ns3::StaWifiMac>();
    if (staMac != nullptr) {
      Address dest = staMac->GetBssid();
      Ssid ssid = staMac->GetSsid();
      NS_LOG_INFO("STA Deassociated: Node " << nodeId << ", Device " << deviceId << " MAC"
                                            << staMac->GetAddress() << ", to MAC: " << maddr
                                            << ", Bssid: " << dest << ", Ssid: " << ssid);

      int i, j;
      Ptr<Node> pNode, pTargetNode;
      pTargetNode = NULL;
      for (i = 0; i < NodeList::GetNNodes(); i++) {
        pNode = NodeList::GetNode(i);
        for (j = 0; j < pNode->GetNDevices(); j++) {
          if (Mac48Address::ConvertFrom(pNode->GetDevice(j)->GetAddress()) == maddr) {
            pTargetNode = pNode;
            i = NodeList::GetNNodes();
            break;
          }
        }
      }
      if (pTargetNode != NULL) {
        NS_LOG_INFO("Node found: " << pTargetNode->GetId());
        // pTargetNode->GetObject<ndn::L3Protocol>()->getForwarder()->m_gone = true;
      }
      else {
        NS_LOG_INFO("Node not found.");
      }
    }
  }
}

/**
 * This scenario simulates a very simple network topology:
 *
 *
 *      +----------+     1Mbps      +--------+     1Mbps      +----------+
 *      | consumer | <------------> | router | <------------> | producer |
 *      +----------+         10ms   +--------+          10ms  +----------+
 *
 *
 * Consumer requests data from producer with frequency 10 interests per second
 * (interests contain constantly increasing sequence number).
 *
 * For every received interest, producer replies with a data packet, containing
 * 1024 bytes of virtual payload.
 *
 * To run scenario and see what is happening, use the following command:
 *
 *     NS_LOG=ndn.Consumer:ndn.Producer ./waf --run=ndn-simple
 */

int
main(int argc, char* argv[])
{
  // Setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Gbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("10000"));

  uint32_t run = 1;

  int mobileSize = 1;
  float speed = 30;
  int stopTime = 100;

  int mobileX = -230;
  int mobileY = -200;

  bool real = false;

  int consumerNode = 0;

  float consumerCbrFreq = 1.0;

  int retxTime = 2;

  CommandLine cmd;

  cmd.AddValue("run", "Run", run);

  cmd.AddValue("speed", "mobile speed m/s", speed);
  cmd.AddValue("size", "# mobile", mobileSize);
  cmd.AddValue("stop", "stop time", stopTime);

  cmd.AddValue("mobileX", "X", mobileX);
  cmd.AddValue("mobileY", "Y", mobileY);

  cmd.AddValue("real", "real", real);

  cmd.AddValue("consumerNode", "", consumerNode);

  cmd.AddValue("consumerCbrFreq", "", consumerCbrFreq);

  cmd.AddValue("retxTime", "retx timer, also set interest lifetime", retxTime);

  cmd.Parse(argc, argv);

  Config::SetGlobal("RngRun", IntegerValue(run));

  Config::SetDefault("ns3::ndn::RttEstimator::InitialEstimation", TimeValue(Seconds(retxTime)));
  string interestLifetime = std::to_string(retxTime) + "s";

  Config::SetDefault("ns3::ndn::RttEstimator::InitialEstimation", TimeValue(Seconds(0.05)));  

  // Read the abilene topology
  // Set up stationary nodes
  NodeContainer nodes;
  AnnotatedTopologyReader topologyReader("", 40);
  topologyReader.SetFileName("src/ndnSIM/examples/topo-abilene-mod.txt");
  nodes = topologyReader.Read();

  nodes.Create(2); // server and RV

  PointToPointHelper p2p;
  p2p.Install(nodes.Get(11), nodes.Get(consumerNode)); // consumer <-> ?
  p2p.Install(nodes.Get(12), nodes.Get(5)); // rv <-> ?

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
  NodeContainer customNodes;
  if (!real) {
    customNodes.Add(nodes);
    posAlloc->Add(Vector(0.0, 0.0, 0.0));
    posAlloc->Add(Vector(0.0, 100.0, 0.0));
    posAlloc->Add(Vector(100.0, -100.0, 0.0));
    posAlloc->Add(Vector(100.0, 0, 0.0));
    posAlloc->Add(Vector(100.0, 100.0, 0.0));
    posAlloc->Add(Vector(100.0, 200.0, 0.0));
    posAlloc->Add(Vector(100.0, -200.0, 0.0));
    posAlloc->Add(Vector(200.0, 0.0, 0.0));
    posAlloc->Add(Vector(200.0, 100.0, 0.0));
    posAlloc->Add(Vector(200.0, -100.0, 0.0));
    posAlloc->Add(Vector(200.0, -200.0, 0.0));
  }
  else {
    customNodes.Add(nodes.Get(11));
    customNodes.Add(nodes.Get(12));
  }

  customNodes.Add(nodes.Get(11));
  customNodes.Add(nodes.Get(12));

  posAlloc->Add(Vector(-190.0, -200.0, 0.0)); // consumer
  posAlloc->Add(Vector(0, 50.0, 0.0));        // rv

  mobility.SetPositionAllocator(posAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(customNodes);

  std::string phyMode("DsssRate1Mbps");
  ////// The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;

  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();

  ////// This is one parameter that matters when using FixedRssLossModel
  ////// set it to zero; otherwise, gain will be added
  // wifiPhy.Set ("RxGain", DoubleValue (0) );

  ////// ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;

  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  // wifiChannel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");

  ////// The below FixedRssLossModel will cause the rss to be fixed regardless
  ////// of the distance between the two stations, and the transmit power
  // wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue(rss));

  ////// the following has an absolute cutoff at distance > range (range == radius)
  wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(100));
  wifiPhy.SetChannel(wifiChannel.Create());
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode),
                               "ControlMode", StringValue(phyMode));

  ////// Setup the rest of the upper mac
  ////// Setting SSID, optional. Modified net-device to get Bssid, mandatory for AP unicast
  Ssid ssid = Ssid("wifi-default");
  // wifi.SetRemoteStationManager ("ns3::ArfWifiManager");

  ////// Add a non-QoS upper mac of STAs, and disable rate control
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
  ////// Active associsation of STA to AP via probing.
  wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(true),
                  "ProbeRequestTimeout", TimeValue(Seconds(0.25)));

  // Create mobile nodes
  NodeContainer mobileNodes;
  mobileNodes.Create(mobileSize);

  wifi.Install(wifiPhy, wifiMac, mobileNodes);

  // Setup mobility model
  Ptr<RandomRectanglePositionAllocator> randomPosAlloc =
    CreateObject<RandomRectanglePositionAllocator>();
  Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
  Ptr<UniformRandomVariable> y = CreateObject<UniformRandomVariable>();
  std::stringstream ss;
  ss << "ns3::UniformRandomVariable[Min=" << speed << "|Max=" << speed << "]";
  if (!real) {
    x->SetAttribute("Min", DoubleValue(-30));
    x->SetAttribute("Max", DoubleValue(230));
    randomPosAlloc->SetX(x);
    y->SetAttribute("Min", DoubleValue(-230));
    y->SetAttribute("Max", DoubleValue(230));
    randomPosAlloc->SetY(y);
    mobility.SetPositionAllocator(randomPosAlloc);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds",
                              RectangleValue(Rectangle(-30, 230, -230, 230)), "Distance",
                              DoubleValue(200), "Speed", StringValue(ss.str()));
  }
  else {
    x->SetAttribute("Min", DoubleValue(mobileX));
    x->SetAttribute("Max", DoubleValue(230));
    randomPosAlloc->SetX(x);
    y->SetAttribute("Min", DoubleValue(-220));
    y->SetAttribute("Max", DoubleValue(-80));
    randomPosAlloc->SetY(y);
    mobility.SetPositionAllocator(randomPosAlloc);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds",
                              RectangleValue(Rectangle(mobileX, 230, -220, -80)), "Distance",
                              DoubleValue(200), "Speed", StringValue(ss.str()));
  }

  // Make mobile nodes move
  mobility.Install(mobileNodes);

  // Setup initial position of mobile node
  // posAlloc = CreateObject<ListPositionAllocator>();
  // posAlloc->Add(Vector(mobileX, mobileY, 0.0));
  // mobility.SetPositionAllocator(posAlloc);
  // mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  // mobility.Install(mobileNodes);

  ////// Setup AP.
  NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default();
  wifiMacHelper.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconGeneration",
                        BooleanValue(true), "BeaconInterval", TimeValue(Seconds(0.1)));
  for (int i = 0; i < 11; i++) {
    wifi.Install(wifiPhy, wifiMacHelper, nodes.Get(i));
  }

  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.Install(nodes);

  ndnHelper.SetOldContentStore("ns3::ndn::cs::Nocache");
  ndnHelper.Install(mobileNodes);

  // ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/multicast");
  ndn::StrategyChoiceHelper::InstallAll<nfd::fw::TraceForwardingStrategy>("/");

  std::string mapPrefix = "/map";

  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.Install(nodes);

  ndnGlobalRoutingHelper.AddOrigins(mapPrefix, nodes.Get(12));

  std::string routerPrefix = "/router";

  for (int i = 0; i < 11; i++) {
    ndnGlobalRoutingHelper.AddOrigins(routerPrefix + "/" + std::to_string(i), nodes.Get(i));
  }

  // Installing applications

  // Stationary server (consumer)
  ndn::AppHelper serverHelper("ns3::ndn::KiteMsConsumer");
  serverHelper.SetAttribute("Prefix", StringValue("/this/is/going/nowhere"));
  serverHelper.SetAttribute("Frequency", DoubleValue(consumerCbrFreq));
  serverHelper.SetAttribute("LifeTime", StringValue(interestLifetime));
  ApplicationContainer serverApp = serverHelper.Install(nodes.Get(11)); // consumer
  serverApp.Stop(Seconds(stopTime - 1));
  serverApp.Start(Seconds(1.0)); // delay start

  // Rendezvous Point
  ndn::AppHelper rvHelper("ns3::ndn::KiteMsServer");
  rvHelper.SetAttribute("Prefix", StringValue(mapPrefix));
  rvHelper.Install(nodes.Get(12));

  // Mobile node, prefix of the mapping server is hardcoded
  ndn::AppHelper mobileNodeHelper("ns3::ndn::KiteMsMobile");
  mobileNodeHelper.SetPrefix(routerPrefix); // this inherits producer, so this is its own prefix
  mobileNodeHelper.SetAttribute("PayloadSize", StringValue("1024")); // the same as consumer window
  ApplicationContainer mobileApp =
    mobileNodeHelper.Install(mobileNodes.Get(0)); // first mobile node
  mobileApp.Stop(Seconds(stopTime - 1));

  ndn::GlobalRoutingHelper::CalculateRoutes();

  // ndn::AppDelayTracer::Install(nodes.Get(11),
  //                              "results/kite-pull-"
  //                              + boost::lexical_cast<string>(speed) + "-"
  //                              + boost::lexical_cast<string>(run) + "-"
  //                              + "map-"
  //                              + "app.txt");

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc",
                  MakeCallback(&StaAssoc));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DeAssoc",
                  MakeCallback(&StaDeAssoc));

  Simulator::Stop(Seconds(stopTime));
  Simulator::Run();

  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
