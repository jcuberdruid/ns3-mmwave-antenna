#include "ns3/core-module.h"
#include "/home/testbed/ns3/json/single_include/nlohmann/json.hpp"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include <ns3/lte-ue-net-device.h>
#include "ns3/mmwave-helper.h"
#include <ns3/buildings-helper.h>
#include "ns3/building-container.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include <ns3/buildings-module.h>
#include "ns3/ns2-mobility-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/envVarTaskID.h"
#include "ns3/mmwave-lte-rrc-protocol-real.h"
#include <fstream>
#include <iostream>
#include <typeinfo>
#include <ctime>
#include <chrono>
#include <map> 

//./waf --run "ns2TraceInterface --traceFile=/home/jason/testbedMultiproc/outputMaster/output_12-53-21_on_30.12.2021/map_2/map_2.tcl --nodeNum=10 --buildingsFile=/home/jason/testbedMultiproc/outputMaster/output_12-53-21_on_30.12.2021/map_2/map2Buildings.json"

using namespace std;
using namespace ns3; 
using namespace mmwave;
using json = nlohmann::json;

auto fileNameSuffix = getEnvVar("NS3_JOB_ID");
string progress = ("progress" + fileNameSuffix + ".txt");
string LegendFileName = ("Legend" + fileNameSuffix + ".txt");
string packetFlowFile = ("packetFlowMonitor" + fileNameSuffix + ".xml");
void timeNow() {
	auto now = std::chrono::system_clock::now();
	std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
	cout << ns3::Simulator::Now() << " " << std::ctime(&nowTime) << endl;
	ofstream progressFile; 
	progressFile.open(progress.c_str(), ios_base::out | ios_base::trunc);	
	progressFile << ns3::Simulator::Now()  << endl; 
	progressFile.close();
	Simulator::Schedule (MilliSeconds (20), &timeNow);		
}

void rntiLog(NodeContainer ueNodes) {
	//netdevice node legend
	ofstream legendFile; 
	NodeContainer::Iterator it; 
	
	legendFile.open(LegendFileName.c_str(), std::ios_base::app);	
	for (it = ueNodes.Begin(); it != ueNodes.End (); ++it){

		Ptr<Node> node = *it; 
		Ptr<MmWaveUeNetDevice> mmuedev = node->GetDevice(0)->GetObject <MmWaveUeNetDevice> ();
		Vector pos = node->GetObject<MobilityModel> ()->GetPosition ();
		Ptr<LteUeRrc> rrcRNTI = mmuedev->GetRrc();
		Ptr<MmWaveEnbNetDevice> enbAttached = mmuedev->GetTargetEnb();
		
      		legendFile << node->GetId() << "," << mmuedev->GetImsi() << "," << enbAttached->GetCellId() << "," << rrcRNTI->GetRnti() <<"," << pos.x << "," << pos.y << endl;
	}
}

int main (int argc, char *argv[]) {

	//defaults		
	string traceFile;
	string configFile;
	string dataRate = "100Gb/s";
	int mtu = 1500; 
	int ueCodeBook = 22;
	int enbCodeBook = 22;
	int nodeNum = 1;
	int enbNum = 2;
	uint32_t bandDiv = 2;
	double delay = 0.010;
	double totalBandwidth = 800e6;
	double frequency = 28e9;	
	double simTime = 5;		
	Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
	bool useCa = true;
	bool useRR = true;

	CommandLine cmd (__FILE__);
	cmd.AddValue ("configFile", "path to configuration json file", configFile);
	cmd.Parse (argc, argv);
	//get user configurations from json file 
	filebuf fb;
	if (fb.open(configFile, ios::in)) {
		istream is(&fb);
		json j = json::parse(is);	
		//ns3Config
		traceFile = j["ns3Params"]["traceFile"];
		ueCodeBook = j["ns3Params"]["ueCodeBook"];
		enbCodeBook = j["ns3Params"]["enbCodeBook"];
		nodeNum = j["ns3Params"]["nodeNum"];
		simTime  = j["ns3Params"]["simTime"];
		bandDiv = j["ns3Params"]["bandDiv"];
		frequency = j["ns3Params"]["frequency"];
		totalBandwidth = j["ns3Params"]["totalBandwidth"];
		dataRate = j["ns3Params"]["dataRate"];
		mtu = j["ns3Params"]["mtu"];
		delay = j["ns3Params"]["delay"];
		double enbHeight = j["ns3Params"]["enbHeight"];
		int roomX = j["ns3Params"]["roomsX"];
		int roomY = j["ns3Params"]["roomsY"];
		int floors = j["ns3Params"]["floors"];
		double zHeight = j["ns3Params"]["buildingHeight"];
		string buildingType = j["ns3Params"]["type"];
		string wallType = j["ns3Params"]["material"];  

	//enb position from json

		enbNum = j["enbs"]["enbNum"];
		auto eArr = j["enbs"]["coordinates"];

		vector<vector<double>> enbsVector; 
		for(auto& x : eArr.items()) {
			vector<double> enb;
			for (auto& i : x.value().items()) { 
				enb.push_back(i.value()); 
			}
			enbsVector.push_back(enb); 
		}
			
		for (vector<double> e: enbsVector) {
			cout << "enbPos " << e[0] << " " << e[1] << endl;
			enbPositionAlloc->Add(Vector(e[0], e[1], enbHeight));
		}

	//buildings config from json 	

		auto bArr = j["buildings"]["coordinates"]; 
		vector<vector<double>> buildingsVector;
		for(auto& x : bArr.items()) {
			vector<double> buildingTmp;
			for(auto& i : x.value().items()) {
				cout << i.value() << endl;
				buildingTmp.push_back(i.value()); 
			}
			buildingsVector.push_back(buildingTmp); 
		}
		BuildingContainer bContainer; 
		auto buildingTypeSet = Building::Residential;

		if (buildingType == "Building::Residential") {
		  	buildingTypeSet = Building::Residential;
		} else if (buildingType == "Building::Office") {
			buildingTypeSet = Building::Office;
		} else if (buildingType == "Building::Commercial") {
			buildingTypeSet = Building::Commercial;
		}	
		auto wallTypeSet = Building::ConcreteWithWindows;

		if (wallType == "Building::Wood") {
		  	wallTypeSet = Building::Wood;
		} else if (wallType == "Building::ConcreteWithWindows") {
			wallTypeSet = Building::ConcreteWithWindows;
		} else if (wallType == "Building::ConcreteWithoutWindows") {
			wallTypeSet = Building::ConcreteWithoutWindows;
		} else if (wallType == "Building::StoneBlocks") {
			wallTypeSet = Building::StoneBlocks;
		}	

		for (vector<double> c: buildingsVector) {
		
			Ptr < Building > building;
			building = Create<Building> ();
			building->SetBoundaries (Box (c[0], c[1], c[2], c[3], zHeight, 0.0));
			building->SetBuildingType (buildingTypeSet);
			building->SetExtWallsType (wallTypeSet);
			building->SetNFloors (floors);
			building->SetNRoomsX (roomX);
			building->SetNRoomsY (roomY);
			
			bContainer.Add(building); 
		}		
	
		fb.close();
	}
		

	//parse UE config
	std::string ueCodeBookPath;
	int ueRows;
	int ueColumns;
	switch(ueCodeBook) {
		case 11:
			ueCodeBookPath = "src/mmwave/model/Codebooks/1x1.txt";
			ueRows = 1;
			ueColumns = 1; 
			break;
		case 12:
			ueCodeBookPath = "src/mmwave/model/Codebooks/1x2.txt";
			ueRows = 1;
			ueColumns = 2; 
			break;
		case 14:
			ueCodeBookPath = "src/mmwave/model/Codebooks/1x4.txt";
			ueRows = 1;
			ueColumns = 4; 
			break;
		case 18:
			ueCodeBookPath = "src/mmwave/model/Codebooks/1x8.txt";
			ueRows = 1;
			ueColumns = 8; 
			break;
		case 21:
			ueCodeBookPath = "src/mmwave/model/Codebooks/2x1.txt";
			ueRows = 2;
			ueColumns = 1; 
			break;
		case 22:
			ueCodeBookPath = "src/mmwave/model/Codebooks/2x2.txt";
			ueRows = 2;
			ueColumns = 2; 
			break;
		case 41:
			ueCodeBookPath = "src/mmwave/model/Codebooks/4x1.txt";
			ueRows = 4;
			ueColumns = 1; 
			break;
		case 44:
			ueCodeBookPath = "src/mmwave/model/Codebooks/4x4.txt";
			ueRows = 4;
			ueColumns = 4; 
			break;
		case 48:
			ueCodeBookPath = "src/mmwave/model/Codebooks/4x8.txt";
			ueRows = 4;
			ueColumns = 8; 
			break;
		case 81:
			ueCodeBookPath = "src/mmwave/model/Codebooks/8x1.txt";
			ueRows = 8;
			ueColumns = 1; 
			break;
		case 84:
			ueCodeBookPath = "src/mmwave/model/Codebooks/8x4.txt";
			ueRows = 8;
			ueColumns = 4; 
			break;
		case 88:
			ueCodeBookPath = "src/mmwave/model/Codebooks/8x8.txt";
			ueRows = 8;
			ueColumns = 8; 
			break;
	}

	
	//parse enb config
	std::string enbCodeBookPath;
	int enbRows;
	int enbColumns;
	switch(enbCodeBook) {
		case 11:
			enbCodeBookPath = "src/mmwave/model/Codebooks/1x1.txt";
			enbRows = 1;
			enbColumns = 1; 
			break;
		case 12:
			enbCodeBookPath = "src/mmwave/model/Codebooks/1x2.txt";
			enbRows = 1;
			enbColumns = 2; 
			break;
		case 14:
			enbCodeBookPath = "src/mmwave/model/Codebooks/1x4.txt";
			enbRows = 1;
			enbColumns = 4; 
			break;
		case 18:
			enbCodeBookPath = "src/mmwave/model/Codebooks/1x8.txt";
			enbRows = 1;
			enbColumns = 8; 
			break;
		case 21:
			enbCodeBookPath = "src/mmwave/model/Codebooks/2x1.txt";
			enbRows = 2;
			enbColumns = 1; 
			break;
		case 22:
			enbCodeBookPath = "src/mmwave/model/Codebooks/2x2.txt";
			enbRows = 2;
			enbColumns = 2; 
			break;
		case 41:
			enbCodeBookPath = "src/mmwave/model/Codebooks/4x1.txt";
			enbRows = 4;
			enbColumns = 1; 
			break;
		case 44:
			enbCodeBookPath = "src/mmwave/model/Codebooks/4x4.txt";
			enbRows = 4;
			enbColumns = 4; 
			break;
		case 48:
			enbCodeBookPath = "src/mmwave/model/Codebooks/4x8.txt";
			enbRows = 4;
			enbColumns = 8; 
			break;
		case 81:
			enbCodeBookPath = "src/mmwave/model/Codebooks/8x1.txt";
			enbRows = 8;
			enbColumns = 1; 
			break;
		case 84:
			enbCodeBookPath = "src/mmwave/model/Codebooks/8x4.txt";
			enbRows = 8;
			enbColumns = 4; 
			break;
		case 88:
			enbCodeBookPath = "src/mmwave/model/Codebooks/8x8.txt";
			enbRows = 8;
			enbColumns = 8; 
			break;
	}
	
	// 1. create MmWavePhyMacCommon object
	Ptr<MmWavePhyMacCommon> phyMacConfig0 = CreateObject<MmWavePhyMacCommon> ();
	phyMacConfig0->SetBandwidth ((1-1/(bandDiv))*totalBandwidth);
	phyMacConfig0->SetNumerology (MmWavePhyMacCommon::Numerology::NrNumerology2);
	phyMacConfig0->SetCentreFrequency (frequency);

	// 2. create the MmWaveComponentCarrier object
	Ptr<MmWaveComponentCarrier> cc0 = CreateObject<MmWaveComponentCarrier> ();
	cc0->SetConfigurationParameters (phyMacConfig0);
	cc0->SetAsPrimary (true);
	
	// CC 1
	Ptr<MmWaveComponentCarrier> cc1;
	if (useCa) {
			// 1. create MmWavePhyMacCommon object
			Ptr<MmWavePhyMacCommon> phyMacConfig1 = CreateObject<MmWavePhyMacCommon> ();
			phyMacConfig1->SetBandwidth (totalBandwidth * 1/ bandDiv);
			phyMacConfig1->SetNumerology (MmWavePhyMacCommon::Numerology::NrNumerology2);
			phyMacConfig1->SetCentreFrequency (73e9);
			phyMacConfig1->SetCcId (1);
			
			// 2. create the MmWaveComponentCarrier object
			cc1 = CreateObject<MmWaveComponentCarrier> ();
			cc1->SetConfigurationParameters (phyMacConfig1);
			cc1->SetAsPrimary (false);
	}

	std::map<uint8_t, MmWaveComponentCarrier> ccMap;
	ccMap [0] = *cc0;	
	if (useCa) {
	      ccMap [1] = *cc1;
	}

	for (uint8_t i = 0; i < ccMap.size (); i++) {
	Ptr<MmWavePhyMacCommon> phyMacConfig = ccMap[i].GetConfigurationParameters();
	std::cout << "Component Carrier " << (uint32_t)(phyMacConfig->GetCcId())
		<< " frequency : " << phyMacConfig->GetCenterFrequency() / 1e9 << " GHz,"
		<< " bandwidth : " << phyMacConfig->GetBandwidth() / 1e6 << " MHz,"
		<< ", numRefSc: " << (uint32_t)phyMacConfig->GetNumRefScPerSym()
		<< std::endl;
	}

	Config::SetDefault ("ns3::MmWaveHelper::UseCa",BooleanValue (useCa));
	if (useCa)
		{
			Config::SetDefault ("ns3::MmWaveHelper::NumberOfComponentCarriers",UintegerValue (2));
			if (useRR)
				{
					Config::SetDefault ("ns3::MmWaveHelper::EnbComponentCarrierManager",StringValue ("ns3::MmWaveRrComponentCarrierManager"));
				}
			else
				{
					Config::SetDefault ("ns3::MmWaveHelper::EnbComponentCarrierManager",StringValue ("ns3::MmWaveBaRrComponentCarrierManager"));
				}
		}
	Config::SetDefault("ns3::MmWaveHelper::PathlossModel", StringValue ("ns3::ThreeGppUmaPropagationLossModel"));

	//set up / configure Mmwave ptr_mmWave 
	
	Ptr<MmWaveHelper> ptr_mmWave = CreateObject<MmWaveHelper> ();
	ptr_mmWave->SetCcPhyParams(ccMap);
	ptr_mmWave->SetChannelConditionModelType ("ns3::BuildingsChannelConditionModel");
	
	// select the beamforming model
	ptr_mmWave->SetBeamformingModelType("ns3::MmWaveCodebookBeamforming");
	
	// configure the UE antennas:
	// 1. specify the path of the file containing the codebook
	ptr_mmWave->SetUeBeamformingCodebookAttribute ("CodebookFilename", StringValue (ueCodeBookPath));
	// 2. set the antenna dimensions
	ptr_mmWave->SetUePhasedArrayModelAttribute ("NumRows",UintegerValue (ueRows));
	ptr_mmWave->SetUePhasedArrayModelAttribute ("NumColumns",UintegerValue (ueColumns));
	
	// configure the BS antennas:
	// 1. specify the path of the file containing the codebook
	ptr_mmWave->SetEnbBeamformingCodebookAttribute ("CodebookFilename", StringValue (enbCodeBookPath));
	ptr_mmWave->SetEnbPhasedArrayModelAttribute ("NumRows", UintegerValue (enbRows));
	// 2. set the antenna dimensions
	ptr_mmWave->SetEnbPhasedArrayModelAttribute ("NumColumns", UintegerValue (enbColumns));

	//ns2 tracefile import 
	Ns2MobilityHelper ns2 = Ns2MobilityHelper (traceFile); 
	
	//create nodes and containers 
	NodeContainer enbNodes; //Enbs
	cout << "enb Num " << enbNum << endl; 
	enbNodes.Create (enbNum);

	NodeContainer ueNodes; //UE  
	ueNodes.Create (nodeNum); 
	ns2.Install(ueNodes.Begin(), ueNodes.End()); 

	// EPC, RemoteHost, Networking
	Ipv4Address remoteHostAddr;
	Ptr<Node> remoteHost;
	InternetStackHelper internet;
	Ptr<MmWavePointToPointEpcHelper> epcHelper;
	epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
	ptr_mmWave->SetEpcHelper (epcHelper);

	// create the Internet by connecting remoteHost to pgw. Setup routing too
	Ptr<Node> pgw = epcHelper->GetPgwNode ();

	// create remotehost
	NodeContainer remoteHostContainer;
	remoteHostContainer.Create (1);
	internet.Install (remoteHostContainer);
	Ipv4StaticRoutingHelper ipv4RoutingHelper;
	Ipv4InterfaceContainer internetIpIfaces;

	remoteHost = remoteHostContainer.Get (0);
	// create the Internet
	PointToPointHelper p2ph;
	p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
	p2ph.SetDeviceAttribute ("Mtu", UintegerValue (mtu)); //Maximum transmission unit fragmentation of packets
	p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (delay)));

	NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);

	Ipv4AddressHelper ipv4h;
	ipv4h.SetBase ("1.0.0.0", "255.255.0.0");
	internetIpIfaces = ipv4h.Assign (internetDevices);

	// interface 0 is localhost, 1 is the p2p device
	remoteHostAddr = internetIpIfaces.GetAddress (1);

	Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
	remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.255.0.0"), 1);

	//mobility set up for nodes 

	MobilityHelper enbmobility;
	enbmobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	enbmobility.SetPositionAllocator (enbPositionAlloc);
	enbmobility.Install (enbNodes);
	BuildingsHelper::Install (enbNodes);	
	BuildingsHelper::Install (ueNodes);

	//net devices
	NetDeviceContainer enbNetDev = ptr_mmWave->InstallEnbDevice (enbNodes);
	NetDeviceContainer ueNetDev = ptr_mmWave->InstallUeDevice (ueNodes);
	
	// install the IP stack on the UEs
	internet.Install (ueNodes);
	Ipv4InterfaceContainer ueIpIface;
	ueIpIface = epcHelper->AssignUeIpv4Address (ueNetDev);
	// assign IP address to UEs, and install applications
	// set the default gateway for the UE
	Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNodes.Get (0)->GetObject<Ipv4> ());
	ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

	// Add X2 interfaces
  	ptr_mmWave->AddX2Interface (enbNodes);
	ptr_mmWave->AttachToClosestEnb (ueNetDev, enbNetDev);

	// Flow monitor
	Ptr<FlowMonitor> flowMonitor;
	FlowMonitorHelper flowHelper;
	flowMonitor = flowHelper.InstallAll();

	// install and start applications on UEs and remote host
	uint16_t dlPort = 1234;
	uint16_t ulPort = 2000;
	ApplicationContainer clientApps;
	ApplicationContainer serverApps;

	uint16_t interPacketInterval = 1; // millisecond 

	PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
	PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));

	NodeContainer::Iterator it; 

	for (it = ueNodes.Begin(); it != ueNodes.End (); ++it){
		Ptr<Node> node = *it; 
		serverApps.Add (dlPacketSinkHelper.Install (node));
	}

	serverApps.Add (ulPacketSinkHelper.Install (remoteHost));

	for (uint32_t i = 0; i < ueNodes.GetN(); ++i){
		UdpClientHelper dlClient (ueIpIface.GetAddress (i), dlPort);
		dlClient.SetAttribute ("Interval", TimeValue (MilliSeconds (interPacketInterval)));
		dlClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
		clientApps.Add (dlClient.Install (remoteHost));
	}
	UdpClientHelper ulClient (remoteHostAddr, ulPort);
	ulClient.SetAttribute ("Interval", TimeValue (MilliSeconds (interPacketInterval)));
	ulClient.SetAttribute ("MaxPackets", UintegerValue (1000000));

	for (it = ueNodes.Begin(); it != ueNodes.End (); ++it){

		Ptr<Node> node = *it; 
		clientApps.Add (ulClient.Install (node));
	}
//	clientApps.Add (ulClient.Install (ueNodes.Get (0)));

	serverApps.Start (Seconds (0.1));
	clientApps.Start (Seconds (0.1));

	//node legend 
	ofstream legendFile; 
        legendFile.open(LegendFileName.c_str(), std::ios_base::app);
        legendFile << "node, IMSI, CellID, RNTI, posx, posy" << endl;
	legendFile.close();

	serverApps.Stop (Seconds (simTime));

	Simulator::Schedule (Seconds(0), &timeNow);		
	for(int i = 0; i < (simTime/.2); i++) {
		Simulator::Schedule (Seconds(0.2*i), &rntiLog, ueNodes);		
	}

	ptr_mmWave->EnableTraces ();
	
	Simulator::Stop (Seconds (simTime));	

	Simulator::Run ();

	flowMonitor->SerializeToXmlFile(packetFlowFile, true, true);
	Simulator::Destroy ();	
	
	//ensure that progress bar stops 
	ofstream progressFile;
        progressFile.open(progress.c_str(), ios_base::out | ios_base::trunc);
        progressFile << "0" << endl;
        progressFile.close();
	
	return 0;

}
