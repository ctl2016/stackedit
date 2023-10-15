#include <iostream>
#include <taskflow/taskflow.hpp>

int main() {
  tf::Taskflow taskflow;
  tf::Executor executor(8);

  auto initGlobal = taskflow.emplace([] { std::cout << "initGlobal" << std::endl; }).name("initGlobal");
  auto startZmqSvr = taskflow.emplace([] { std::cout << "StartZmqSvr" << std::endl; }).name("StartZmqSvr");
  auto chkOtaEvt = taskflow.emplace([] { 
    if(1) {
        std::cout << "ChkOtaEvt 0" << std::endl;
        return 0;
    }
    else {
        std::cout << "ChkOtaEvt 1" << std::endl;
        return 1;
    }
  }).name("chkOtaEvt");

  auto chkActState = taskflow.emplace([] { std::cout << "ChkActState" << std::endl; }).name("chkActState");

  auto startAct = taskflow.emplace(
    [&] (tf::Subflow& subflow) {

      auto actSoc = subflow.emplace([&]() {
        printf("  Subtask actSoc\n");
      }).name("actSoc");

      auto actMcu = subflow.emplace([&]() {
        printf("  Subtask actMcu\n");
      }).name("actMcu");

      // detach or join the subflow (by default the subflow join at B)
      subflow.join();
      //subflow.detach();
    }
  ).name("startAct");

  auto startFlash = taskflow.emplace(
    [&] (tf::Subflow& subflow) {

      auto flashSoc = subflow.emplace([&]() {
        printf("  Subtask flashSoc\n");
      }).name("flashSoc");

      auto flashMcu = subflow.emplace([&]() {
        printf("  Subtask flashMcu\n");
      }).name("flashMcu");

      // detach or join the subflow (by default the subflow join at B)
      subflow.join();
      //subflow.detach();
    }
  ).name("startFlash");

  auto reboot = taskflow.emplace([] { std::cout << "Reboot" << std::endl; }).name("reboot");

  initGlobal.precede(chkActState, startZmqSvr);
  chkActState.precede(chkOtaEvt);
  chkOtaEvt.precede(startFlash, startAct);
  startAct.precede(reboot);
  
  executor.run(taskflow).get();  // block until finished

  // examine the graph
  taskflow.dump(std::cout);

  return 0;
}
