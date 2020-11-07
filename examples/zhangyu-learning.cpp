/* -*-  张宇，ns3和opengym联合仿真。 -*-
 * 借鉴思路，利用ZMQ实现ns3反向调用python的代码，重新实现原来的python调用ns3
 * 合并了原来的ndnSIM-multipath和opengym 2020-8-27
 *
*/

#include "ns3/core-module.h"
#include "ns3/opengym-module.h"
// for ns3 default
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/ndnSIM-module.h"
//
#include "ns3/names.h"
#include "ns3/string.h"
#include "ns3/ptr.h"

//#include "q-learning-forwarding/learning-strategy.hpp"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("OpenGym");

/*
Define observation space
*/

Ptr<OpenGymSpace> MyGetObservationSpace(void)
{
  uint32_t nodeNum = 5;
  float low = 0.0;
  float high = 10.0;
  std::vector<uint32_t> shape = {nodeNum,};
  std::string dtype = TypeNameGet<uint32_t> ();
  Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace> (low, high, shape, dtype);
  NS_LOG_UNCOND ("MyGetObservationSpace: " << space);
  return space;
}

/*
Define action space
*/
Ptr<OpenGymSpace> MyGetActionSpace(void)
{
  uint32_t nodeNum = 5;

  Ptr<OpenGymDiscreteSpace> space = CreateObject<OpenGymDiscreteSpace> (nodeNum);
  NS_LOG_UNCOND ("MyGetActionSpace: " << space);

  //ZhangYu dataType
  Ptr<OpenGymDiscreteSpace> discrete = CreateObject<OpenGymDiscreteSpace> (nodeNum);
  Ptr<OpenGymDictSpace> zyspace = CreateObject<OpenGymDictSpace> ();
  zyspace->Add("node1 to node2", discrete);
  zyspace->Add("node2 to node3", discrete);

  Ptr<OpenGymDiscreteContainer> discreteCt = CreateObject<OpenGymDiscreteContainer>(nodeNum);
  discreteCt->SetValue(10);

  Ptr<OpenGymTupleContainer> data = CreateObject<OpenGymTupleContainer> ();
  data->Add(discreteCt);

  return zyspace;
}

/*
Define game over condition
*/
bool MyGetGameOver(void)
{

  bool isGameOver = false;
  bool test = false;
  static float stepCounter = 0.0;
  stepCounter += 1;
  if (stepCounter == 10 && test) {
      isGameOver = true;
  }
  NS_LOG_UNCOND ("MyGetGameOver: " << isGameOver);
  return isGameOver;
}

/*
Collect observations
*/
Ptr<OpenGymDataContainer> MyGetObservation(void)
{
  uint32_t nodeNum = 5;
  uint32_t low = 0.0;
  uint32_t high = 10.0;
  Ptr<UniformRandomVariable> rngInt = CreateObject<UniformRandomVariable> ();

  std::vector<uint32_t> shape = {nodeNum,};
  Ptr<OpenGymBoxContainer<uint32_t> > box = CreateObject<OpenGymBoxContainer<uint32_t> >(shape);

  // generate random data
  for (uint32_t i = 0; i<nodeNum; i++){
    uint32_t value = rngInt->GetInteger(low, high);
    box->AddValue(value);
  }

  NS_LOG_UNCOND ("MyGetObservation: " << box);
  return box;
}

/*
Define reward function
*/
float MyGetReward(void)
{
  static float reward = 0.0;
  reward += 1;
  return reward;
}

/*
Define extra info. Optional
*/
std::string MyGetExtraInfo(void)
{
  std::string myInfo = "testInfo";
  myInfo += "|123";
  NS_LOG_UNCOND("MyGetExtraInfo: " << myInfo);
  return myInfo;
}


/*
Execute received actions
*/
bool MyExecuteActions(Ptr<OpenGymDataContainer> action)
{
  Ptr<OpenGymDiscreteContainer> discrete = DynamicCast<OpenGymDiscreteContainer>(action);
  NS_LOG_UNCOND ("MyExecuteActions: " << action);
  return true;
}

void ScheduleNextStateRead(double envStepTime, Ptr<OpenGymInterface> openGym)
{
  Simulator::Schedule (Seconds(envStepTime), &ScheduleNextStateRead, envStepTime, openGym);
  openGym->NotifyCurrentState();
}

/*
ZhangYu 2020-8-27
All the parameters for the Network Simulation
*/

void networksConfiguration(bool manualAssign,int InterestsPerSec,int simulationSpan,int TraceSpan,int recordsNumber,string routingName)
{
	//----------------仿真拓扑----------------
	AnnotatedTopologyReader topologyReader ("", 20);
	// current directory is build, same as include
	topologyReader.SetFileName ("src/ndnSIM/examples/topologies/topo-for-CompareMultiPath.txt");
	// 如果把topologies copy to scratch, then simple_test.py will be stuck, because topologies folder was copied to build
	topologyReader.Read ();
	int nodesNumber=topologyReader.GetNodes().size();
	const NodeContainer& allNodes=topologyReader.GetNodes();
	for (const auto& n : allNodes) {
		std::cout << Names::FindName(n) << std::endl;
	}
	std::list<TopologyReader::Link> allLinks=topologyReader.GetLinks();
	for (TopologyReader::ConstLinksIterator iter=allLinks.begin(); iter!=allLinks.end(); iter++) {
		std::cout << "FromNode:"<< iter->GetFromNode()->GetId() << " FromNodeName:" << iter->GetFromNodeName() << std::endl;
	}

	//----------------安装Install CCNx stack ----------------
	ns3::ndn::StackHelper ndnHelper;
	ndnHelper.setPolicy("nfd::cs::lru");
	ndnHelper.setCsSize(1);
	ndnHelper.InstallAll();

	topologyReader.ApplyOspfMetric();  //使得链路metric生效

	//----------------Installing global routing interface on all nodes ----------------
	ns3::ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
	ndnGlobalRoutingHelper.InstallAll ();

	//----------------设置节点的业务 ----------------
	//根据不同的拓扑手工指定或者自动生成业务节点对
	std::vector<int> consumerNodes,producerNodes;
	//生成consumer和producer的节点号动态数组
	int tmpConsumer[]={0};
	int tmpProducer[]={3};
	consumerNodes.assign(tmpConsumer,tmpConsumer+sizeof(tmpConsumer)/sizeof(int));
	producerNodes.assign(tmpProducer,tmpProducer+sizeof(tmpConsumer)/sizeof(int));
	//根据上面生成的节点对编号装载应用
	for (uint32_t i = 0; i < consumerNodes.size(); i++) {
		ns3::ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
		consumerHelper.SetAttribute("Frequency", StringValue (boost::lexical_cast<std::string>(InterestsPerSec)));
		//ndn::AppHelper consumerHelper("ns3::ndn::ConsumerZipfMandelbrot");
		//consumerHelper.SetAttribute("NumberOfContents", StringValue("100")); // 10 different contents
		//可以选择的有：
		//"none": no randomization
		//"uniform": uniform distribution in range (0, 1/Frequency)
		//"exponential": exponential distribution with mean 1/Frequency
		consumerHelper.SetAttribute("Randomize", StringValue("exponential"));

		Ptr<Node> consumer1 = Names::Find<Node> ("Node"+boost::lexical_cast<std::string> (consumerNodes[i]));
		consumerHelper.SetPrefix ("/Node"+boost::lexical_cast<std::string>(consumerNodes[i]));
		ApplicationContainer app=consumerHelper.Install(consumer1);
		app.Start(Seconds(0.01*i));
		// Choosing forwarding strategy 
		//
		// ***************!!!!!!!要特别注意，这里的转发策略名字写错了，不会报错，还可以以默认的转发策略执行
		//
		ns3::ndn::StrategyChoiceHelper::InstallAll("/Node"+boost::lexical_cast<std::string> (consumerNodes[i]), "/localhost/nfd/strategy/learning");
		//ns3::ndn::StrategyChoiceHelper::InstallAll("/Node"+boost::lexical_cast<std::string> (consumerNodes[i]), "/localhost/nfd/strategy/randomized-rounding");
		//ns3::ndn::StrategyChoiceHelper::InstallAll("/Node"+boost::lexical_cast<std::string> (consumerNodes[i]), "/localhost/nfd/strategy/best-route");
		//ns3::ndn::StrategyChoiceHelper::InstallAll("/Node"+boost::lexical_cast<std::string> (consumerNodes[i]), "/localhost/nfd/strategy/ncc");

		NS_LOG_UNCOND ("ZhangYu  consumer1->GetId(): " << consumer1->GetId() << "  prefix: /Node" <<boost::lexical_cast<std::string>(consumerNodes[i]));
	}

	for(uint32_t i=0;i<producerNodes.size();i++)	{
		ns3::ndn::AppHelper producerHelper ("ns3::ndn::Producer");
		producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));
		//认为producer节点的Prefix和对应位置的consumer节点一致
		producerHelper.SetPrefix ("/Node"+boost::lexical_cast<std::string>(consumerNodes[i]));

		Ptr<Node> producer1 = Names::Find<Node> ("Node"+boost::lexical_cast<std::string> (producerNodes[i]));
		ndnGlobalRoutingHelper.AddOrigins ("/Node"+boost::lexical_cast<std::string>(consumerNodes[i]), producer1);
		producerHelper.Install(producer1);
		NS_LOG_UNCOND ("ZhangYu producer1->GetId(): " <<producer1->GetId());
	}


	// Calculate and install FIBs
	if(routingName.compare("BestRoute")==0){
	  ns3::ndn::GlobalRoutingHelper::CalculateRoutes ();
	}
	else if(routingName.compare("Learning")==0){
			ns3::ndn::GlobalRoutingHelper::addRouteHop("Node0","/Node0","Node3",1,0.5);
			ns3::ndn::GlobalRoutingHelper::addRouteHop("Node0","/Node0","Node2",1,0.5);
			ns3::ndn::GlobalRoutingHelper::addRouteHop("Node2","/Node0","Node3",1,1.0);
	}
	

	//----------------Tracer 仿真结果 ----------------
	//ZhangYu Add the trace，不愿意文件名称还有大小写的区别，所以把 routingName 全部转为小写
	std::transform(routingName.begin(), routingName.end(), routingName.begin(), ::tolower);
	string filename="-"+routingName+"-"+boost::lexical_cast<std::string>(InterestsPerSec)+".txt";

	TraceSpan=simulationSpan/recordsNumber;
	if(TraceSpan<1)
		TraceSpan=1;
	ns3::ndn::CsTracer::InstallAll ("Results/cs-trace"+filename, Seconds (TraceSpan));
	ns3::ndn::L3RateTracer::InstallAll ("Results/rate-trace"+filename, Seconds (TraceSpan));
	// L3AggregateTracer disappeared in new version
	//ndn::L3AggregateTracer::InstallAll ("aggregate-trace-"+filename, Seconds (1));
	ns3::ndn::AppDelayTracer::InstallAll ("Results/app-delays-trace"+filename);
	L2RateTracer::InstallAll ("Results/drop-trace"+filename, Seconds (TraceSpan));
}

int
main (int argc, char *argv[])
{
	bool manualAssign=true;
	int InterestsPerSec=200;
	int simulationSpan=5;
	int TraceSpan=1;
	int recordsNumber=100;
	string routingName="Learning";

	// Parameters of the scenario
	uint32_t simSeed = 1;
	double simulationTime = 10; //seconds
	double envStepTime = 0.1; //seconds, ns3gym env step time interval
	uint32_t openGymPort = 5555;
	uint32_t testArg = 0;

	// 这里的参数是在 start_sim.py中传给的
	CommandLine cmd;
	// required parameters for OpenGym interface
	cmd.AddValue ("openGymPort", "Port number for OpenGym env. Default: 5555", openGymPort);
	cmd.AddValue ("simSeed", "Seed for random generator. Default: 1", simSeed);
	// optional parameters
	cmd.AddValue ("simTime", "Simulation time in seconds. Default: 10s", simulationTime);
	cmd.AddValue ("testArg", "Extra simulation argument. Default: 0", testArg);

	// 19年仿真版本带过来的参数
	cmd.AddValue("InterestsPerSec","Interests emit by consumer per second",InterestsPerSec);
	cmd.AddValue("simulationSpan","Simulation span time by seconds",simulationSpan);
	cmd.AddValue ("routingName", "could be Flooding, BestRoute, k-shortest, MultiPathPairFirst, debug", routingName);
	cmd.AddValue ("recordsNumber", "total number of records in tracer file", recordsNumber);
	cmd.Parse (argc, argv);

	NS_LOG_UNCOND("Ns3Env parameters:");
	NS_LOG_UNCOND("--simulationTime: " << simulationTime);
	NS_LOG_UNCOND("--openGymPort: " << openGymPort);
	NS_LOG_UNCOND("--envStepTime: " << envStepTime);
	NS_LOG_UNCOND("--seed: " << simSeed);
	NS_LOG_UNCOND("--testArg: " << testArg);

	RngSeedManager::SetSeed (1);
	RngSeedManager::SetRun (simSeed);

	networksConfiguration(manualAssign,InterestsPerSec,simulationSpan,TraceSpan,recordsNumber,routingName);

	// OpenGym Env
	Ptr<OpenGymInterface> openGym = CreateObject<OpenGymInterface> (openGymPort);
	openGym->SetGetActionSpaceCb( MakeCallback (&MyGetActionSpace) );
	openGym->SetGetObservationSpaceCb( MakeCallback (&MyGetObservationSpace) );
	openGym->SetGetGameOverCb( MakeCallback (&MyGetGameOver) );
	openGym->SetGetObservationCb( MakeCallback (&MyGetObservation) );
	openGym->SetGetRewardCb( MakeCallback (&MyGetReward) );
	openGym->SetGetExtraInfoCb( MakeCallback (&MyGetExtraInfo) );
	openGym->SetExecuteActionsCb( MakeCallback (&MyExecuteActions) );

	//Simulator::Schedule (Seconds(0.0), &ScheduleNextStateRead, envStepTime, openGym);

	NS_LOG_UNCOND ("Simulation start");
	Simulator::Stop (Seconds (simulationTime));
	Simulator::Run ();
	NS_LOG_UNCOND ("Simulation stop");

	openGym->NotifySimulationEnd();
	Simulator::Destroy ();
	return 0;
}

/*
int
main (int argc, char *argv[])
{
	bool manualAssign=true;
	int InterestsPerSec=200;
	int simulationSpan=5;
	int TraceSpan=1;
	int recordsNumber=100;
	string routingName="debug";

	//----------------命令行参数----------------
	CommandLine cmd;
	cmd.AddValue("InterestsPerSec","Interests emit by consumer per second",InterestsPerSec);
	cmd.AddValue("simulationSpan","Simulation span time by seconds",simulationSpan);
	cmd.AddValue ("routingName", "could be Flooding, BestRoute, k-shortest, MultiPathPairFirst, debug", routingName);
	cmd.AddValue ("recordsNumber", "total number of records in tracer file", recordsNumber);
	cmd.Parse(argc,argv);
	//std::cout << "routingName: " << routingName << "   " << InterestsPerSec << " " << simulationSpan << std::endl;

	//----------------仿真拓扑----------------
	AnnotatedTopologyReader topologyReader ("", 20);
	//topologyReader.SetFileName ("src/ndnSIM/examples/topologies/26node-result.txt");
	topologyReader.SetFileName ("src/ndnSIM/examples/topologies/topo-for-CompareMultiPath.txt");
	topologyReader.Read ();
	int nodesNumber=topologyReader.GetNodes().size();

	//----------------安装Install CCNx stack ----------------
	ns3::ndn::StackHelper ndnHelper;
	// 下面这一句是Install NDN stack on all nodes
	ndnHelper.setPolicy("nfd::cs::lru");
	ndnHelper.setCsSize(1);
	ndnHelper.InstallAll();

	topologyReader.ApplyOspfMetric();  //使得链路metric生效

	//----------------Installing global routing interface on all nodes ----------------
	ns3::ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
	ndnGlobalRoutingHelper.InstallAll ();

	//----------------设置节点的业务 ----------------
	//根据不同的拓扑手工指定或者自动生成业务节点对
	std::vector<int> consumerNodes,producerNodes;
	//生成consumer和producer的节点号动态数组
	if(manualAssign)	{
		int tmpConsumer[]={0};
		int tmpProducer[]={3};
		consumerNodes.assign(tmpConsumer,tmpConsumer+sizeof(tmpConsumer)/sizeof(int));
		producerNodes.assign(tmpProducer,tmpProducer+sizeof(tmpConsumer)/sizeof(int));
	}
	else	{
		for(int i=0;i<nodesNumber/2;i++)	{
		  consumerNodes.push_back(i);
		  producerNodes.push_back(i+nodesNumber/2);
		}
	}

	//根据上面生成的节点对编号装载应用
	for(uint32_t i=0;i<consumerNodes.size();i++)	{
		ns3::ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
		consumerHelper.SetAttribute("Frequency", StringValue (boost::lexical_cast<std::string>(InterestsPerSec)));        // 100 interests a second
		//ndn::AppHelper consumerHelper("ns3::ndn::ConsumerZipfMandelbrot");
		//consumerHelper.SetAttribute("NumberOfContents", StringValue("100")); // 10 different contents
		//可以选择的有：
		//"none": no randomization
		//"uniform": uniform distribution in range (0, 1/Frequency)
		//"exponential": exponential distribution with mean 1/Frequency
		consumerHelper.SetAttribute("Randomize", StringValue("exponential"));

		Ptr<Node> consumer1 = Names::Find<Node> ("Node"+boost::lexical_cast<std::string> (consumerNodes[i]));
		consumerHelper.SetPrefix ("/Node"+boost::lexical_cast<std::string>(consumerNodes[i]));
		ApplicationContainer app=consumerHelper.Install(consumer1);
		app.Start(Seconds(0.01*i));
		// Choosing forwarding strategy
		ns3::ndn::StrategyChoiceHelper::InstallAll("/Node"+boost::lexical_cast<std::string> (consumerNodes[i]), "/localhost/nfd/strategy/randomized-rounding");
		//ndn::StrategyChoiceHelper::InstallAll("/Node"+boost::lexical_cast<std::string> (consumerNodes[i]), "/localhost/nfd/strategy/best-route");
		//ndn::StrategyChoiceHelper::InstallAll("/Node"+boost::lexical_cast<std::string> (consumerNodes[i]), "/localhost/nfd/strategy/ncc");

		std::cout <<"ZhangYu  consumer1->GetId(): " <<consumer1->GetId() << "  prefix: /Node"+boost::lexical_cast<std::string>(consumerNodes[i]) << std::endl;
	}

	for(uint32_t i=0;i<producerNodes.size();i++)	{
		ns3::ndn::AppHelper producerHelper ("ns3::ndn::Producer");
		producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));
		//认为producer节点的Prefix和对应位置的consumer节点一致
		producerHelper.SetPrefix ("/Node"+boost::lexical_cast<std::string>(consumerNodes[i]));

		Ptr<Node> producer1 = Names::Find<Node> ("Node"+boost::lexical_cast<std::string> (producerNodes[i]));
		ndnGlobalRoutingHelper.AddOrigins ("/Node"+boost::lexical_cast<std::string>(consumerNodes[i]), producer1);
		producerHelper.Install(producer1);
		std::cout <<"ZhangYu producer1->GetId(): " <<producer1->GetId() << std::endl;
	}

	// Calculate and install FIBs
	if(routingName.compare("BestRoute")==0){
	  ns3::ndn::GlobalRoutingHelper::CalculateRoutes ();
	}
	else if(routingName.compare("debug")==0){
		//当Consumer是0时，prefix=/Node0时，需要添加 0-->1-->4 的路由才可以，添加反向4->1->0没有Traffic
		ns3::ndn::GlobalRoutingHelper::addRouteHop("Node0","/Node0","Node3",1,0.5);
		ns3::ndn::GlobalRoutingHelper::addRouteHop("Node0","/Node0","Node2",1,0.5);
		ns3::ndn::GlobalRoutingHelper::addRouteHop("Node2","/Node0","Node3",1,1.0);
	}
	else if(routingName.compare("Flooding")==0){
		ns3::ndn::GlobalRoutingHelper::CalculateAllPossibleRoutes();
	}
	else{
		std::cout << "!!!!  ~~~~~~Unkown routingName: " << routingName << ", try again..." <<std::endl;
	}

	// The failure of the link connecting consumer and router will start from seconds 10.0 to 15.0
	//Simulator::Schedule (Seconds (10.0), ndn::LinkControlHelper::FailLink, Names::Find<Node> ("Node0"),Names::Find<Node> ("Node4"));
	//Simulator::Schedule (Seconds (15.0), ndn::LinkControlHelper::UpLink,   Names::Find<Node> ("Node0"),Names::Find<Node> ("Node4"));

	Simulator::Stop (Seconds(simulationSpan));

	//ZhangYu Add the trace，不愿意文件名称还有大小写的区别，所以把 routingName 全部转为小写
	std::transform(routingName.begin(), routingName.end(), routingName.begin(), ::tolower);
	string filename="-"+routingName+"-"+boost::lexical_cast<std::string>(InterestsPerSec)+".txt";
	//filename=".txt";

	TraceSpan=simulationSpan/recordsNumber;
	if(TraceSpan<1)
		TraceSpan=1;
	ns3::ndn::CsTracer::InstallAll ("Results/cs-trace"+filename, Seconds (TraceSpan));
	ns3::ndn::L3RateTracer::InstallAll ("Results/rate-trace"+filename, Seconds (TraceSpan));
	// L3AggregateTracer disappeared in new version
	//ndn::L3AggregateTracer::InstallAll ("Results/aggregate-trace-"+filename, Seconds (1));
	ns3::ndn::AppDelayTracer::InstallAll ("Results/app-delays-trace"+filename);
	L2RateTracer::InstallAll ("Results/drop-trace"+filename, Seconds (TraceSpan));

	Simulator::Run ();
	Simulator::Destroy ();

  return 0;
}
*/
