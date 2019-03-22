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

#include "apps/kite-upload-server.hpp"
#include "apps/kite-upload-mobile.hpp"

#include "ns3/ndnSIM/NFD/daemon/fw/trace-forwarding.hpp"

#include <boost/algorithm/string.hpp>

NS_LOG_COMPONENT_DEFINE("kite.upload");

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
StaAssociation(string context, Mac48Address maddr)
{
  vector<string> tokens;
  boost::split(tokens, context, boost::is_any_of("/"));
  int nodeId = atoi(tokens[2].c_str());
  int deviceId = atoi(tokens[4].c_str());
  NS_LOG_INFO("STA Associated: Node " << nodeId << ", Device "
                                      << deviceId /* << " MAC" << mac->GetAddress()*/
                                      << ", to MAC: " << maddr << ", context: " << context);

  Ptr<Node> node = NodeList::GetNode(nodeId);

  Ptr<ns3::WifiNetDevice> wifiDev = node->GetDevice(deviceId)->GetObject<ns3::WifiNetDevice>();
  if (wifiDev != nullptr) {
    Ptr<ns3::StaWifiMac> staMac = wifiDev->GetMac()->GetObject<ns3::StaWifiMac>();
    if (staMac != nullptr) {
      Address dest = staMac->GetBssid();
      Ssid ssid = staMac->GetSsid();
      NS_LOG_INFO("Node " << nodeId << ", Device " << deviceId << ", Mac: " << staMac->GetAddress()
                          << ", Bssid: " << dest << ", Ssid: " << ssid);
      ndn::KiteUploadMobile* mobileApp =
        dynamic_cast<ndn::KiteUploadMobile*>(&(*node->GetApplication(0)));
      mobileApp->OnAssociation();
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
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("20ms"));
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("10"));

  uint32_t run = 1;

  int mobileSize = 1;
  float speed = 30;
  int stopTime = 100;

  float traceLifetime = 1;
  float refreshInterval = 1;

  bool doPull = false;
  bool prolongTrace = false;

  string interestLifetime = "1s";

  CommandLine cmd;

  cmd.AddValue("run", "Run", run);

  cmd.AddValue("speed", "mobile speed m/s", speed);
  cmd.AddValue("size", "# mobile", mobileSize);
  cmd.AddValue("stop", "stop time", stopTime);

  cmd.AddValue("traceLifetime", "trace lifetime", traceLifetime); // set TI's lifetime, which will
                                                                  // be used by forwarder to set the
                                                                  // lifetime of trace
  cmd.AddValue("refreshInterval", "refresh interval", refreshInterval); // set to 0 to disable
                                                                        // periodic sending, always
                                                                        // send after relocation

  cmd.AddValue("doPull", "enable pulling", doPull);
  cmd.AddValue("prolongTrace", "extend trace lifetime on dataflow", prolongTrace);

  cmd.AddValue("interestLifetime", "lifetime of consumer Interest", interestLifetime);

  cmd.Parse(argc, argv);

  Config::SetGlobal("RngRun", IntegerValue(run));

  Config::SetDefault("ns3::ndn::L3Protocol::DoPull", BooleanValue(doPull));

  // Read the abilene topology
  // Set up stationary nodes
  NodeContainer nodes;
  AnnotatedTopologyReader topologyReader("", 25);
  topologyReader.SetFileName("src/ndnSIM/examples/topo-abilene.txt");
  nodes = topologyReader.Read();

  nodes.Create(2); // server and RV

  PointToPointHelper p2p;
  p2p.Install(nodes.Get(11), nodes.Get(0));
  p2p.Install(nodes.Get(12), nodes.Get(1));

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
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

  posAlloc->Add(Vector(10.0, 10.0, 0.0));
  posAlloc->Add(Vector(10.0, 110.0, 0.0));

  mobility.SetPositionAllocator(posAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

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
  x->SetAttribute("Min", DoubleValue(100));
  x->SetAttribute("Max", DoubleValue(200));
  randomPosAlloc->SetX(x);
  Ptr<UniformRandomVariable> y = CreateObject<UniformRandomVariable>();
  y->SetAttribute("Min", DoubleValue(-200));
  y->SetAttribute("Max", DoubleValue(200));
  randomPosAlloc->SetY(y);

  mobility.SetPositionAllocator(randomPosAlloc);
  std::stringstream ss;
  ss << "ns3::UniformRandomVariable[Min=" << speed << "|Max=" << speed << "]";

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds",
                            RectangleValue(Rectangle(100, 200, -100, 200)), "Distance",
                            DoubleValue(100), "Speed", StringValue(ss.str()));

  // Make mobile nodes move
  mobility.Install(mobileNodes);

  // Setup initial position of mobile node
  posAlloc = CreateObject<ListPositionAllocator>();
  posAlloc->Add(Vector(200.0, -100.0, 0.0));
  mobility.SetPositionAllocator(posAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(mobileNodes);

  ////// Setup AP.
  NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default();
  wifiMacHelper.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconGeneration",
                        BooleanValue(true), "BeaconInterval", TimeValue(Seconds(0.1)));
                        // BooleanValue(false));
  for (int i = 0; i < 11; i++) {
    wifi.Install(wifiPhy, wifiMacHelper, nodes.Get(i));
  }

  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.InstallAll();

  // ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/multicast");
  ndn::StrategyChoiceHelper::InstallAll<nfd::fw::TraceForwardingStrategy>("/");

  std::string serverPrefix = "/server";
  std::string dataPrefix = "/alice/photo";
  std::string rvPrefix = "/rv";

  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.Install(nodes);

  ndnGlobalRoutingHelper.AddOrigins(serverPrefix, nodes.Get(11));
  ndnGlobalRoutingHelper.AddOrigins(rvPrefix, nodes.Get(12));

  // Installing applications

  // Stationary server
  ndn::AppHelper serverHelper("ns3::ndn::KiteUploadServer");
  serverHelper.SetAttribute("ServerPrefix", StringValue(serverPrefix));
  serverHelper.SetAttribute("LifeTime", StringValue(interestLifetime));
  ApplicationContainer serverApp = serverHelper.Install(nodes.Get(11)); // first node
  serverApp.Stop(Seconds(stopTime - 1));

  // Rendezvous Point
  ndn::AppHelper rvHelper("ns3::ndn::KiteRv");
  rvHelper.SetAttribute("RvPrefix", StringValue(rvPrefix));
  rvHelper.Install(nodes.Get(12));

  // Mobile node
  ndn::AppHelper mobileNodeHelper("ns3::ndn::KiteUploadMobile");
  mobileNodeHelper.SetPrefix(rvPrefix
                             + dataPrefix); // this inherits producer, so this is its own prefix
  mobileNodeHelper.SetAttribute("RvPrefix", StringValue(rvPrefix));
  mobileNodeHelper.SetAttribute("ServerPrefix", StringValue(serverPrefix));
  mobileNodeHelper.SetAttribute("DataPrefix", StringValue(dataPrefix)); // this corresponds to the
                                                                        // producer prefix, the full
                                                                        // data prefix is rv prefix
                                                                        // + producer prefix
  mobileNodeHelper.SetAttribute("PayloadSize", StringValue("1024"));
  mobileNodeHelper.SetAttribute("TraceLifetime", StringValue(std::to_string(traceLifetime)));
  mobileNodeHelper.SetAttribute("RefreshInterval", StringValue(std::to_string(refreshInterval)));
  ApplicationContainer mobileApp =
    mobileNodeHelper.Install(mobileNodes.Get(0)); // first mobile node
  mobileApp.Stop(Seconds(stopTime - 1));

  ndn::GlobalRoutingHelper::CalculateRoutes();

  L2RateTracer::InstallAll("drop-trace.txt");
  ndn::L3RateTracer::Install(nodes.Get(11), "rate-trace.txt");
  ndn::AppDelayTracer::Install(nodes.Get(11), "app-delays-trace.txt");

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc",
                  MakeCallback(&StaAssociation));

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
