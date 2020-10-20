/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2020 SIGNET Lab, Department of Information Engineering,
 * University of Padova
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * This example shows how to configure a full stack simulation using the
 * QdChannelModel.
 * The simulation involves two nodes moving in an empty rectangular room
 * and communicates through a wireless channel at 60 GHz with a bandwidth
 * of about 400 MHz.
 */

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/three-gpp-spectrum-propagation-loss-model.h"
#include "ns3/simple-net-device.h"
#include "ns3/node-container.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/qd-channel-model.h"
#include "ns3/mmwave-helper.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/config-store.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/isotropic-antenna-model.h"
#include "ns3/three-gpp-antenna-model.h"
#include <fstream>

NS_LOG_COMPONENT_DEFINE ("QdChannelModelExample");

using namespace ns3;
using namespace mmwave;

void ComputeSinr (Ptr<NetDevice> thisEnbDevice, Ptr<NetDevice> otherUeDevice, 
                  Ptr<NetDevice> interfEnbDevice, Ptr<NetDevice> interfUeDevice,
                  Ptr<SpectrumValue> txPsd, Ptr<SpectrumValue> noisePsd)
{  
  Ptr<MmWaveSpectrumPhy> enbSpectrumPhy = DynamicCast<MmWaveEnbNetDevice> (thisEnbDevice)->GetPhy ()->GetDlSpectrumPhy ();
  Ptr<MmWaveSpectrumPhy> ueSpectrumPhy = DynamicCast<MmWaveUeNetDevice> (otherUeDevice)->GetPhy ()->GetDlSpectrumPhy ();
  enbSpectrumPhy->ConfigureBeamforming (otherUeDevice);
  // ueSpectrumPhy->ConfigureBeamforming (thisEnbDevice);
  
  Ptr<MmWaveSpectrumPhy> interfEnbSpectrumPhy = DynamicCast<MmWaveEnbNetDevice> (interfEnbDevice)->GetPhy ()->GetDlSpectrumPhy ();
  Ptr<MmWaveSpectrumPhy> interfUeSpectrumPhy = DynamicCast<MmWaveUeNetDevice> (interfUeDevice)->GetPhy ()->GetDlSpectrumPhy ();
  interfEnbSpectrumPhy->ConfigureBeamforming (interfUeDevice);
  // interfUeSpectrumPhy->ConfigureBeamforming (thisEnbDevice);
  
  // retrieve the PropagationLossModel and the SpectrumPropagationLossModel
  Ptr<SpectrumChannel> sc = enbSpectrumPhy->GetSpectrumChannel ();
  Ptr<SpectrumPropagationLossModel> splm = sc->GetSpectrumPropagationLossModel ();
  
  // compute the sinr
  Ptr<MobilityModel> enbMm = thisEnbDevice->GetNode ()->GetObject<MobilityModel> ();
  Ptr<MobilityModel> ueMm = otherUeDevice->GetNode ()->GetObject<MobilityModel> ();
  Ptr<MobilityModel> interfMm = interfEnbDevice->GetNode ()->GetObject<MobilityModel> ();
  
  Ptr<SpectrumValue> rxPsd = splm->CalcRxPowerSpectralDensity (txPsd, enbMm, ueMm);
  Ptr<SpectrumValue> interfPsd = splm->CalcRxPowerSpectralDensity (txPsd, interfMm, ueMm);
  
  double snr = 10 * log10 (Sum (*rxPsd) / Sum (*noisePsd));
  double sinr = 10 * log10 (Sum (*rxPsd) / (Sum (*noisePsd) + Sum (*interfPsd)));
  
  std::ofstream f;
  f.open ("sinr-trace.txt", std::ios::app);
  f << Simulator::Now ().GetSeconds () << " " << snr << " " << sinr << '\n';
  f.close ();  
}

int
main (int argc, char *argv[])
{
  std::string qdFilesPath = "contrib/qd-channel/model/QD/"; // The path of the folder with the QD scenarios
  std::string codebookFilesPath = "src/mmwave/model/Codebooks/"; // The path of the folder with the codebooks are
  std::string scenario = "ParkingLotCars"; // The name of the scenario
  uint32_t interPacketInterval = 1e3; // App inter packet arrival [us]
  double txPower = 30.0; // Transmitted power for both eNB and UE [dBm]
  double noiseFigure = 9.0; // Noise figure for both eNB and UE [dB]
  uint16_t enbAntennaNumRows = 4; // The number of rows for the gNBs antenna arrays
  uint16_t enbAntennaNumColumns = 4; // The number of columns for the gNBs antenna arrays
  uint16_t ueAntennaNumRows = 1; // The number of rows for the UE antenna arrays
  uint16_t ueAntennaNumColumns = 4; // The number of columns for the UE antenna arrays
  uint32_t appPacketSize = 1460; // Application packet size [B]
  std::string bfModelType = "ns3::MmWaveSvdBeamforming"; // Beamforming model type
  double cbUpdatePeriod = 1.0; // Refresh period for updating the beam pairs [ms]
  std::string enbAntennaType = "ns3::ThreeGppAntennaModel"; // The type of antenna model for eNBs
  std::string ueAntennaType = "ns3::IsotropicAntennaModel"; // The type of antenna model for UEs
  
  CommandLine cmd;
  cmd.AddValue ("qdFilesPath", "The path of the folder with the QD scenarios", qdFilesPath);
  cmd.AddValue ("codebookFilesPath", "path of the folder with the codebooks are", codebookFilesPath);
  cmd.AddValue ("scenario", "The name of the scenario", scenario);
  cmd.AddValue ("ipi", "App inter packet arrival [us]", interPacketInterval);
  cmd.AddValue ("txPower", "Transmitted power for both eNB and UE [dBm]", txPower);
  cmd.AddValue ("noiseFigure", "Noise figure for both eNB and UE [dB]", noiseFigure);
  cmd.AddValue ("enbAntennaNumRows", "The number of rows for the gNBs antenna arrays", enbAntennaNumRows);
  cmd.AddValue ("enbAntennaNumColumns", "The number of columns for the gNBs antenna arrays", enbAntennaNumColumns);
  cmd.AddValue ("ueAntennaNumRows", "The number of rows for the UE antenna arrays", ueAntennaNumRows);
  cmd.AddValue ("ueAntennaNumColumns", "The number of columns for the UE antenna arrays", ueAntennaNumColumns);
  cmd.AddValue ("appPacketSize", "Application packet size [B]", appPacketSize);
  cmd.AddValue ("bfModelType", "Beamforming model type", bfModelType);
  cmd.AddValue ("cbUpdatePeriod", "Refresh period for updating the beam pairs [ms]", cbUpdatePeriod);
  cmd.AddValue ("enbAntennaType", "The type of antenna model", enbAntennaType);
  cmd.AddValue ("ueAntennaType", "The type of antenna model", ueAntennaType);
  cmd.Parse (argc, argv);

  // Setup
  LogComponentEnableAll (LOG_PREFIX_ALL);

  bool harqEnabled = true;
  bool rlcAmEnabled = true;

  Config::SetDefault ("ns3::MmWaveHelper::RlcAmEnabled", BooleanValue (rlcAmEnabled));
  Config::SetDefault ("ns3::MmWaveHelper::HarqEnabled", BooleanValue (harqEnabled));
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::HarqEnabled", BooleanValue (harqEnabled));
  Config::SetDefault ("ns3::MmWaveCodebookBeamforming::UpdatePeriod", TimeValue (MilliSeconds (cbUpdatePeriod)));
  Config::SetDefault ("ns3::CosineAntennaModel::VerticalBeamwidth", DoubleValue (180)); 
  Config::SetDefault ("ns3::CosineAntennaModel::HorizontalBeamwidth", DoubleValue (180)); 

  // Create the tx and rx nodes
  NodeContainer ueNodes;
  NodeContainer enbNodes;
  enbNodes.Create (2);
  ueNodes.Create (2);

  // Create the tx and rx mobility models, set the positions to be equal to the
  // initial positions of the nodes in the ray tracer
  Ptr<MobilityModel> enb0Mob = CreateObject<ConstantPositionMobilityModel> ();
  Ptr<MobilityModel> enb1Mob = CreateObject<ConstantPositionMobilityModel> ();
  Ptr<MobilityModel> ue0Mob = CreateObject<ConstantPositionMobilityModel> ();
  Ptr<MobilityModel> ue1Mob = CreateObject<ConstantPositionMobilityModel> ();
  
  if (scenario == "ParkingLot-old")
  {
    enb0Mob->SetPosition (Vector (22,  32,   3));
    enb1Mob->SetPosition (Vector (32, -37,   3));
    ue0Mob->SetPosition  (Vector (40,  50, 1.6));
    ue1Mob->SetPosition  (Vector ( 0,   0, 1.5));
  }
  else if (scenario == "ParkingLotCars")
  {
    enb0Mob->SetPosition (Vector (40,  55,   3));
    enb1Mob->SetPosition (Vector (55, -13,   3));
    ue0Mob->SetPosition  (Vector (40,  56, 1.5));
    ue1Mob->SetPosition  (Vector (20,  15, 1.5));
  }
  else if (scenario == "L-Room")
  {
    enb0Mob->SetPosition (Vector (0.1,    3, 2.5));
    enb1Mob->SetPosition (Vector (  8, 18.8, 2.5));
    ue0Mob->SetPosition  (Vector (0.5,    3, 1.5));
    ue1Mob->SetPosition  (Vector (  8,  2.5, 1.5));
  }
  else 
  {
    NS_FATAL_ERROR ("Unsupported scenario");
  }
  
  // Assign the mobility models to the nodes
  enbNodes.Get (0)->AggregateObject (enb0Mob);
  enbNodes.Get (1)->AggregateObject (enb1Mob);
  ueNodes.Get (0)->AggregateObject (ue0Mob);
  ueNodes.Get (1)->AggregateObject (ue1Mob);

  // Configure the channel
  Config::SetDefault ("ns3::MmWaveHelper::PathlossModel", StringValue (""));
  Config::SetDefault ("ns3::MmWaveHelper::ChannelModel", StringValue ("ns3::ThreeGppSpectrumPropagationLossModel"));
  Ptr<QdChannelModel> qdModel = CreateObject<QdChannelModel> (qdFilesPath, scenario);
  Time simTime = qdModel->GetQdSimTime ();
  Config::SetDefault ("ns3::ThreeGppSpectrumPropagationLossModel::ChannelModel", PointerValue (qdModel));

  // Set power and noise figure
  Config::SetDefault ("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue (400e6));
  Config::SetDefault ("ns3::MmWaveEnbPhy::TxPower", DoubleValue (txPower));
  Config::SetDefault ("ns3::MmWaveEnbPhy::NoiseFigure", DoubleValue (noiseFigure));
  Config::SetDefault ("ns3::MmWaveUePhy::TxPower", DoubleValue (txPower));
  Config::SetDefault ("ns3::MmWaveUePhy::NoiseFigure", DoubleValue (noiseFigure));

  // Create the MmWave helper
  Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper> ();
  
  // set the beamforming model
  mmwaveHelper->SetBeamformingModelType (bfModelType);
  
  // set the antenna type
  ObjectFactory enbAntennaModelFactory = ObjectFactory (enbAntennaType);
  ObjectFactory ueAntennaModelFactory = ObjectFactory (ueAntennaType);
  mmwaveHelper->SetEnbPhasedArrayModelAttribute ("AntennaElement", PointerValue (enbAntennaModelFactory.Create<AntennaModel> ()));
  mmwaveHelper->SetUePhasedArrayModelAttribute ("AntennaElement", PointerValue (ueAntennaModelFactory.Create<AntennaModel> ()));
  
  // set the number of antennas in the devices
  mmwaveHelper->SetUePhasedArrayModelAttribute ("NumRows" , UintegerValue (ueAntennaNumRows));
  mmwaveHelper->SetUePhasedArrayModelAttribute ("NumColumns" , UintegerValue (ueAntennaNumColumns));
  mmwaveHelper->SetEnbPhasedArrayModelAttribute ("NumRows" , UintegerValue (enbAntennaNumRows));
  mmwaveHelper->SetEnbPhasedArrayModelAttribute ("NumColumns" , UintegerValue (enbAntennaNumColumns));

  mmwaveHelper->SetUeBeamformingCodebookAttribute ("CodebookFilename", StringValue (codebookFilesPath + 
                                                                                    std::to_string (ueAntennaNumRows) + "x" + 
                                                                                    std::to_string (ueAntennaNumColumns) + ".txt"));
  mmwaveHelper->SetEnbBeamformingCodebookAttribute ("CodebookFilename", StringValue (codebookFilesPath + 
                                                                                     std::to_string (enbAntennaNumRows) + "x" + 
                                                                                     std::to_string (enbAntennaNumColumns) + ".txt"));    

  mmwaveHelper->SetSchedulerType ("ns3::MmWaveFlexTtiMacScheduler");
  Ptr<MmWavePointToPointEpcHelper>  epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
  mmwaveHelper->SetEpcHelper (epcHelper);
  mmwaveHelper->SetHarqEnabled (harqEnabled);

  // Create a single RemoteHost
  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  // Interface 0 is localhost, 1 is the p2p device
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  // Install MmWaveDevices
  NetDeviceContainer ueMmWaveDevs = mmwaveHelper->InstallUeDevice (ueNodes);
  NetDeviceContainer enbMmWaveDevs;
  if (scenario == "ParkingLot-old" ||
      scenario == "ParkingLotCars")
  {
    // eNBs
    mmwaveHelper->SetEnbPhasedArrayModelAttribute ("BearingAngle" , DoubleValue (DegreesToRadians (-76.2107)));
    mmwaveHelper->SetEnbPhasedArrayModelAttribute ("DowntiltAngle" , DoubleValue (DegreesToRadians (12.0)));
    NetDeviceContainer enbMmWaveDev0 = mmwaveHelper->InstallEnbDevice (NodeContainer (enbNodes.Get (0)));

    mmwaveHelper->SetEnbPhasedArrayModelAttribute ("BearingAngle" , DoubleValue (DegreesToRadians (105.826)));
    mmwaveHelper->SetEnbPhasedArrayModelAttribute ("DowntiltAngle" , DoubleValue (DegreesToRadians (12.0)));
    NetDeviceContainer enbMmWaveDev1 = mmwaveHelper->InstallEnbDevice (NodeContainer (enbNodes.Get (1)));
    
    enbMmWaveDevs = NetDeviceContainer (enbMmWaveDev0, enbMmWaveDev1);
  }
  else if (scenario == "L-Room")
  {
    // eNBs
    mmwaveHelper->SetEnbPhasedArrayModelAttribute ("BearingAngle" , DoubleValue (DegreesToRadians (-90.0)));
    mmwaveHelper->SetEnbPhasedArrayModelAttribute ("DowntiltAngle" , DoubleValue (DegreesToRadians (12.0)));
    NetDeviceContainer enbMmWaveDev0 = mmwaveHelper->InstallEnbDevice (NodeContainer (enbNodes.Get (0)));

    mmwaveHelper->SetEnbPhasedArrayModelAttribute ("BearingAngle" , DoubleValue (DegreesToRadians (0.0)));
    mmwaveHelper->SetEnbPhasedArrayModelAttribute ("DowntiltAngle" , DoubleValue (DegreesToRadians (12.0)));
    NetDeviceContainer enbMmWaveDev1 = mmwaveHelper->InstallEnbDevice (NodeContainer (enbNodes.Get (1)));
    
    enbMmWaveDevs = NetDeviceContainer (enbMmWaveDev0, enbMmWaveDev1);
  }
  else 
  {
    NS_FATAL_ERROR ("Unsupported scenario: " << scenario);
  }
  
  // create the psd of the transmitted signal and of the noise
  Ptr<MmWavePhyMacCommon> mwpmc = DynamicCast<MmWaveEnbNetDevice> (enbMmWaveDevs.Get (0))->GetPhy ()->GetConfigurationParameters ();
  std::vector <int> activeRbs;
  for (uint32_t i = 0; i < mwpmc->GetNumChunks (); i++)
    {
      activeRbs.push_back (i);
    }
  Ptr<SpectrumValue> txPsd = MmWaveSpectrumValueHelper::CreateTxPowerSpectralDensity (mwpmc, txPower, activeRbs);
  Ptr<SpectrumValue> noisePsd = MmWaveSpectrumValueHelper::CreateNoisePowerSpectralDensity (mwpmc, noiseFigure);
  
  Time timeStep = MicroSeconds (5000);
  for (auto i = 0; i < simTime.GetMicroSeconds () / timeStep.GetMicroSeconds (); i++)
  {
    Simulator::Schedule (timeStep * i + timeStep / 2 , &ComputeSinr, enbMmWaveDevs.Get (0), ueMmWaveDevs.Get (0), 
                                                                     enbMmWaveDevs.Get (1), ueMmWaveDevs.Get (1), 
                                                                     txPsd, noisePsd);
  }

  // prepare the output file
  std::ofstream f;
  f.open ("sinr-trace.txt");
  f << "time[s] snr[dB] sinr[dB]\n";
  f.close ();
  
  Simulator::Stop (simTime);
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
