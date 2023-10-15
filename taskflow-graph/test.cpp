#include <iostream>
#include <unistd.h>
#include <taskflow/taskflow.hpp>

int main() {
    tf::Taskflow taskflow;
    tf::Executor executor(8);
    int i = 0;

    auto initGlobal = taskflow.emplace([] { std::cout << "initGlobal" << std::endl; }).name("initGlobal");
    auto startZmqSvr = taskflow.emplace([] { std::cout << "StartZmqSvr" << std::endl; }).name("StartZmqSvr");
    auto chkOtaEvt = taskflow.emplace([&] { 
            sleep(1);
            if(i <= 3) {
                std::cout << "ChkOtaEvt 0, i:" << i << std::endl;
                return 0;
            }
            else {
                std::cout << "ChkOtaEvt 1, i:" << i << std::endl;
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

    auto loopDo = taskflow.emplace([&] { 
                std::cout << "loopDo, i:" << i << std::endl;
            }).name("loopDo");
    
    auto loopWhile = taskflow.emplace([&] { 
                ++i;
                std::cout << "loopWhile, i:" << i << std::endl;
                return 0;
            }).name("loopWhile");


    auto reboot = taskflow.emplace([] { std::cout << "Reboot" << std::endl; }).name("reboot");

    initGlobal.precede(chkActState, startZmqSvr);
    chkActState.precede(loopDo);
    loopDo.precede(chkOtaEvt);
    chkOtaEvt.precede(startFlash, startAct);
    startAct.precede(reboot);
    reboot.precede(loopWhile);
    startFlash.precede(loopWhile);
    loopWhile.precede(chkOtaEvt);

    executor.run(taskflow).get();  // block until finished
    taskflow.dump(std::cout);

    return 0;
}

