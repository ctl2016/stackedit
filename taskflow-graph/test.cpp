#include <iostream>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <taskflow/taskflow.hpp>

int main() {
    tf::Taskflow taskflow;
    tf::Executor executor;
    std::atomic<int> counter(0);
    bool resultAct = false;

    std::cout << "main thrd:" << std::this_thread::get_id() << "\n";

    auto initGlobal = taskflow.emplace([] { std::cout << "initGlobal, thrd: " << std::this_thread::get_id() << std::endl; }).name("initGlobal");
    auto startZmqSvr = taskflow.emplace([] { std::cout << "StartZmqSvr thrd:" << std::this_thread::get_id() << std::endl; }).name("StartZmqSvr");
    auto chkOtaEvt = taskflow.emplace([&] { 
            std::cout << "chkOtaEvt thrd:" << std::this_thread::get_id() << "\n";
            counter.fetch_add(1);
            int i = counter.load();
            sleep(1);
            if(i <= 5) {
                std::cout << "ChkOtaEvt (i <= 5), i:" << i << std::endl;
                return 0;
            }
            else {
                std::cout << "ChkOtaEvt (i = 6), i:" << i << std::endl;
                return 1;
            }
    }).name("chkOtaEvt");

    auto chkActState = taskflow.emplace([] { std::cout << "ChkActState, thrd:" << std::this_thread::get_id() << std::endl; }).name("chkActState");

    auto activate = taskflow.emplace(
            [&] (tf::Subflow& subflow) {

            subflow.emplace([&]() {
                    std::cout << "  Subtask actSoc, thrd:" << std::this_thread::get_id() << "\n";
                    }).name("actSoc");

            subflow.emplace([&]() {
                    std::cout << "  Subtask actMcu, thrd:" << std::this_thread::get_id() << "\n";
                    }).name("actMcu");

            // detach or join the subflow (by default the subflow join at B)
            subflow.join();
            //subflow.detach();
            resultAct = true;
            }
            ).name("activate");

    auto flash = taskflow.emplace(
            [&] (tf::Subflow& subflow) {

                subflow.emplace([&]() {
                        std::cout << "  Subtask flashSoc, thrd:" << std::this_thread::get_id() << "\n";
                }).name("flashSoc");

                subflow.emplace([&]() {
                        std::cout << "  Subtask flashMcu, thrd:" << std::this_thread::get_id() << "\n";
                }).name("flashMcu");
                
                // detach or join the subflow (by default the subflow join at B)
                subflow.join();
                //subflow.detach();
            }
            ).name("flash");

    auto endFlash = taskflow.emplace([&] { 
                std::cout << "endFlash, thrd:" << std::this_thread::get_id() << "\n";
                return 0;
            }).name("endFlash");

    auto endAct = taskflow.emplace([&] { 
                std::cout << "endAct, thrd:" << std::this_thread::get_id() << "\n";
                return resultAct ? 1 : 0;
            }).name("endAct");

    auto reboot = taskflow.emplace([&] { std::cout << "Reboot\n"; }).name("reboot");

    initGlobal.precede(chkActState, startZmqSvr);
    chkActState.precede(chkOtaEvt);

    chkOtaEvt.precede(flash, activate);

    flash.precede(endFlash);
    activate.precede(endAct);

    endFlash.precede(chkOtaEvt);
    endAct.precede(chkOtaEvt, reboot);

    executor.run(taskflow).get();  // block until finished
    taskflow.dump(std::cout);

    return 0;
}