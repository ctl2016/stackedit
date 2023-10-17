#include <iostream>
#include <unistd.h>
#include <atomic>
#include <taskflow/taskflow.hpp>

int main() {
    tf::Taskflow taskflow;
    tf::Executor executor;
    std::atomic<int> counter(0);

    auto initGlobal = taskflow.emplace([] { std::cout << "initGlobal" << std::endl; }).name("initGlobal");
    auto startZmqSvr = taskflow.emplace([] { std::cout << "StartZmqSvr" << std::endl; }).name("StartZmqSvr");
    auto chkOtaEvt = taskflow.emplace([&] { 
            counter.fetch_add(1);
            int i = counter.load();
            // sleep(1);
            if(i <= 5) {
                std::cout << "ChkOtaEvt (i <= 5), i:" << i << std::endl;
                return 0;
            }
            else if(i == 6) {
                std::cout << "ChkOtaEvt (i = 6), i:" << i << std::endl;
                return 1;
            }
            else {
                std::cout << "ChkOtaEvt (i > 6), i:" << i << std::endl;
            }
            return 2;
    }).name("chkOtaEvt");

    auto chkActState = taskflow.emplace([] { std::cout << "ChkActState" << std::endl; }).name("chkActState");

    auto activate = taskflow.emplace(
            [&] (tf::Subflow& subflow) {

            subflow.emplace([&]() {
                    std::cout << "  Subtask actSoc\n";
                    }).name("actSoc");

            subflow.emplace([&]() {
                    std::cout << "  Subtask actMcu\n";
                    }).name("actMcu");

            // detach or join the subflow (by default the subflow join at B)
            subflow.join();
            //subflow.detach();
            }
            ).name("activate");

    auto flash = taskflow.emplace(
            [&] (tf::Subflow& subflow) {

                subflow.emplace([&]() {
                        std::cout << "  Subtask flashSoc\n";
                }).name("flashSoc");

                subflow.emplace([&]() {
                        std::cout << "  Subtask flashMcu\n";
                }).name("flashMcu");
                
                // detach or join the subflow (by default the subflow join at B)
                subflow.join();
                //subflow.detach();
            }
            ).name("flash");

    auto endFlash = taskflow.emplace([&] { 
                std::cout << "endFlash\n";
                return 0;
            }).name("endFlash");

    auto endAct = taskflow.emplace([&] { 
                std::cout << "endAct\n";
                return 0;
            }).name("endAct");

    //auto reboot = subflow.emplace([&] { std::cout << "Reboot\n"; }).name("reboot");
    //reboot.succeed(actSoc, actMcu);

    initGlobal.precede(chkActState, startZmqSvr);
    chkActState.precede(chkOtaEvt);

    chkOtaEvt.precede(flash, activate);

    flash.precede(endFlash);
    activate.precede(endAct);
    
    endFlash.precede(chkOtaEvt);
    endAct.precede(chkOtaEvt);

    executor.run(taskflow).get();  // block until finished
    taskflow.dump(std::cout);

    return 0;
}
