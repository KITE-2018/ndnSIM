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

#include "ns3/ndnSIM-module.h"

#include "apps/kite-upload-server.hpp"
#include "apps/kite-upload-mobile.hpp"
#include "apps/kite-rv.hpp"

#include "ns3/ndnSIM/NFD/daemon/fw/trace-forwarding.hpp"

namespace ns3 {

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

void linkDown() {
	std::cerr << "The link is down\n";
}

int
main(int argc, char* argv[])
{
	//NS_LOG_INFO ("Simulation test");

	// LogComponentEnable("ndn.Producer", LOG_LEVEL_INFO);
	// LogComponentEnable("ndn.Consumer", LOG_LEVEL_INFO);

	// LogComponentEnable("ndn.kite.KiteUploadServer", LOG_LEVEL_INFO);
	// LogComponentEnable("ndn.kite.KiteUploadMobile", LOG_LEVEL_INFO);

	// LogComponentEnable("nfd.Strategy", LOG_LEVEL_INFO);
	// LogComponentEnable("nfd.Forwarder", LOG_LEVEL_INFO);
	// LogComponentEnable("nfd.TraceForwardingStrategy", LOG_LEVEL_INFO);

	// Setting default parameters for PointToPoint links and channels
	Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
	Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("20ms"));
	Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("10"));

	// int isKite = 1;
	// int gridSize = 3;
	int mobileSize = 1;
	int speed = 10;
	// int stopTime = 100;
	// int joinTime = 1;

	CommandLine cmd;
	// cmd.AddValue("kite", "enable Kite", isKite);
	// cmd.AddValue("speed", "mobile speed m/s", speed);
	// cmd.AddValue("size", "# mobile", mobileSize);
	// cmd.AddValue("grid", "grid size", gridSize);
	// cmd.AddValue("stop", "stop time", stopTime);
	// cmd.AddValue("join", "join period", joinTime);
	cmd.Parse (argc, argv);

	std::string phyMode ("DsssRate1Mbps");

	////// disable fragmentation, RTS/CTS for frames below 2200 bytes and fix non-unicast data rate
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

  ////// The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;

  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();

  ////// This is one parameter that matters when using FixedRssLossModel
  ////// set it to zero; otherwise, gain will be added
  // wifiPhy.Set ("RxGain", DoubleValue (0) );

  ////// ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;

	wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  // wifiChannel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");

  ////// The below FixedRssLossModel will cause the rss to be fixed regardless
  ////// of the distance between the two stations, and the transmit power
  // wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue(rss));

  ////// the following has an absolute cutoff at distance > range (range == radius)
  wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel",
                                  "MaxRange", DoubleValue(100));
  wifiPhy.SetChannel (wifiChannel.Create ());
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue (phyMode),
                                "ControlMode", StringValue (phyMode));

  ////// Setup the rest of the upper mac
  ////// Setting SSID, optional. Modified net-device to get Bssid, mandatory for AP unicast
  Ssid ssid = Ssid ("wifi-default");
  // wifi.SetRemoteStationManager ("ns3::ArfWifiManager");

  ////// Add a non-QoS upper mac of STAs, and disable rate control
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  ////// Active associsation of STA to AP via probing.
  wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (true),
                   "ProbeRequestTimeout", TimeValue(Seconds(0.25)));

	// Set up stationary nodes
	NodeContainer nodes;
	nodes.Create(4);

	MobilityHelper mobility;
	Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
	posAlloc->Add (Vector (0.0, 0.0, 0.0));
	posAlloc->Add (Vector (100.0, 0.0, 0.0));
	posAlloc->Add (Vector (200.0, 100.0, 0.0));
	posAlloc->Add (Vector (200.0, -100.0, 0.0));
	mobility.SetPositionAllocator (posAlloc);
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (nodes);

	PointToPointHelper p2p;
	p2p.Install(nodes.Get(0), nodes.Get(1));
	p2p.Install(nodes.Get(1), nodes.Get(2));
	p2p.Install(nodes.Get(1), nodes.Get(3));

	// Create mobile nodes
	NodeContainer mobileNodes;
	mobileNodes.Create (mobileSize);

	wifi.Install (wifiPhy, wifiMac, mobileNodes);

	// Setup mobility model
	Ptr<RandomRectanglePositionAllocator> randomPosAlloc = CreateObject<RandomRectanglePositionAllocator> ();
	Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
	x->SetAttribute ("Min", DoubleValue (200));
	x->SetAttribute ("Max", DoubleValue (250));
	randomPosAlloc->SetX (x);
    Ptr<UniformRandomVariable> y = CreateObject<UniformRandomVariable> ();
	y->SetAttribute ("Min", DoubleValue (-100));
	y->SetAttribute ("Max", DoubleValue (100));
	randomPosAlloc->SetY (y);

	mobility.SetPositionAllocator(randomPosAlloc);
	std::stringstream ss;
	ss << "ns3::UniformRandomVariable[Min=" << speed << "|Max=" << speed << "]";

	mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
		"Bounds", RectangleValue (Rectangle (200, 250, -100, 100)),
		"Distance", DoubleValue (200),
		"Speed", StringValue (ss.str ()));

	// Make mobile nodes move
	mobility.Install (mobileNodes);

	// Setup initial position of mobile node
	posAlloc = CreateObject<ListPositionAllocator> ();
	posAlloc->Add (Vector (200.0, 80.0, 0.0));
	mobility.SetPositionAllocator (posAlloc);
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (mobileNodes);

	//apply wifi component on mobile-nodes and constant-nodes
	// WifiHelper wifi;
	// wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
	// // Set to a non-QoS upper mac
	// NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
	// // Set it to adhoc mode
	// wifiMac.SetType ("ns3::AdhocWifiMac");
	// // wifiMac.SetLinkUpCallback(linkup);
	// // Set Wi-Fi rate manager
	// std::string phyMode ("OfdmRate54Mbps");
	// wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode",StringValue (phyMode), "ControlMode",StringValue (phyMode));
	//
	// YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
	// // ns-3 supports RadioTap and Prism tracing extensions for 802.11
	// wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
	// YansWifiChannelHelper wifiChannel;
	// wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
	// // wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel", "Exponent", DoubleValue (3.0));
	// wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel", "MaxRange", DoubleValue (100.0));
	// wifiPhy.SetChannel (wifiChannel.Create ());
	// wifi.Install (wifiPhy, wifiMac, mobileNodes);
	// Callback<void> linkup;
	// linkup = MakeCallback (&linkUp);
	// mobileNodes.Get(0)->GetObject<ns3::StaWifiMac>()->SetLinkUpCallback(linkup);
	//wifi.Install (wifiPhy, wifiMac, nodes.Get(2));
	//wifi.Install (wifiPhy, wifiMac, nodes.Get(3));

	////// Setup AP.
	NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default ();
	wifiMacHelper.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid),
									"BeaconGeneration", BooleanValue(false));

	wifi.Install (wifiPhy, wifiMacHelper, nodes.Get(2));
	wifi.Install (wifiPhy, wifiMacHelper, nodes.Get(3));

	ndn::StackHelper ndnHelper;
	ndnHelper.SetDefaultRoutes(true);
	ndnHelper.InstallAll();

	// ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/multicast");
	ndn::StrategyChoiceHelper::InstallAll<nfd::fw::TraceForwardingStrategy>("/");
	// ndn::StrategyChoiceHelper::Install<nfd::fw::TraceForwardingStrategy>(nodes.Get(1), "/");
	// ndn::StrategyChoiceHelper::Install<nfd::fw::TraceForwardingStrategy>(nodes.Get(2), "/");
	// ndn::StrategyChoiceHelper::Install<nfd::fw::TraceForwardingStrategy>(nodes.Get(3), "/");

  std::string serverPrefix = "/server";
  std::string mobilePrefix = "/alice";
  std::string dataPrefix = "/alice/photo";
	std::string rvPrefix = "/rv";

	ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
	ndnGlobalRoutingHelper.Install(nodes);

	ndnGlobalRoutingHelper.AddOrigins(serverPrefix, nodes.Get(0));
	ndnGlobalRoutingHelper.AddOrigins(mobilePrefix, nodes.Get(1));
	ndnGlobalRoutingHelper.AddOrigins(rvPrefix, nodes.Get(1));

	// ndn::FibHelper::AddRoute (nodes.Get(1), serverPrefix, nodes.Get(0), 1);
	// ndn::FibHelper::AddRoute (nodes.Get(1), mobilePrefix, nodes.Get(2), 1);
	// ndn::FibHelper::AddRoute (nodes.Get(1), mobilePrefix, nodes.Get(3), 1);

	// Installing applications

  // Stationary server
	ndn::AppHelper serverHelper("ns3::ndn::KiteUploadServer");
	serverHelper.SetAttribute("ServerPrefix", StringValue(serverPrefix));
	serverHelper.Install(nodes.Get(0)); // first node

	// Rendezvous Point
	ndn::AppHelper rvHelper("ns3::ndn::KiteUploadRv");
	rvHelper.SetAttribute("RvPrefix", StringValue(rvPrefix));
	rvHelper.SetAttribute("MobilePrefix", StringValue(mobilePrefix));
	rvHelper.Install(nodes.Get(1));

	// Mobile node
	ndn::AppHelper mobileNodeHelper("ns3::ndn::KiteUploadMobile");
	mobileNodeHelper.SetPrefix(mobilePrefix); // this inherits producer, so this is its own prefix
	mobileNodeHelper.SetAttribute("RvPrefix", StringValue(rvPrefix));
	mobileNodeHelper.SetAttribute("ServerPrefix", StringValue(serverPrefix));
	mobileNodeHelper.SetAttribute("DataPrefix", StringValue(dataPrefix));
	mobileNodeHelper.SetAttribute("PayloadSize", StringValue("1024"));
	// mobileNodeHelper.SetAttribute("TraceLifetime", StringValue("0.5s"));
	// mobileNodeHelper.SetAttribute("RefreshInterval", StringValue("0.5s"));
	mobileNodeHelper.Install(mobileNodes.Get(0)); // first mobile node

	ndn::GlobalRoutingHelper::CalculateRoutes();

	L2RateTracer::InstallAll("drop-trace.txt");
	ndn::L3RateTracer::InstallAll("rate-trace.txt");
	ndn::AppDelayTracer::InstallAll("app-delays-trace.txt");

	// Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc", MakeCallback(&ndn::KiteUploadMobile::Association));

	Simulator::Stop(Seconds(20.0));
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
