#include <iostream>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <taskflow/taskflow.hpp>

void flash()
{
    tf::Taskflow taskflow;
    tf::Executor executor;
    std::atomic<int> counter(0);
    bool resultAct = false;

    std::cout << "main thrd:" << std::this_thread::get_id() << "\n";

    auto initGlobal = taskflow.emplace([] { std::cout << "initGlobal, thrd: " << std::this_thread::get_id() << std::endl; }).name("initGlobal");
    auto startZmqSvr = taskflow.emplace([] { std::cout << "StartZmqSvr thrd:" << std::this_thread::get_id() << std::endl; sleep(20);}).name("StartZmqSvr");
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
}

#include <initializer_list>
#include <cstdarg>
#include <list>

class TaskModule {

public:

    TaskModule()
    {
    }

    TaskModule(const std::string& strName)
    {
        m_strName = strName;
    }

    virtual void Run()
    {
        std::cout << "Run(" << m_strName << ")";
    }

    virtual int32_t RunCond()
    {
        std::cout << "RunCond(" << m_strName << ")";
        return 0;
    }

    const std::string& Name() const
    {
        return m_strName;
    }

    TaskModule& operator >> (TaskModule& task)
    {
        std::cout << m_strName << " >> " << task.Name() << "\n";
        return task;
    }

    TaskModule& Cond(const std::initializer_list<TaskModule>& tasks)
    {
        for (const auto t : tasks)
        {
            std::cout << m_strName << " Cond >> " << t.Name() << "\n";
            m_lstCond.push_back(&t);
        }

        return *this;
    }

    TaskModule& Before(const std::initializer_list<TaskModule>& tasks)
    {
        for (const auto& t : tasks)
        {
            std::cout << m_strName << " Before >> " << t.Name() << "\n";
            m_lst.push_back(&t);
        }

        return *this;
    }

private:
    std::string m_strName;
    std::list<const TaskModule*> m_lstCond;
    std::list<const TaskModule*> m_lst;
};

class TaskExecutor {

public:

    TaskExecutor(const std::initializer_list<TaskModule>& tasks)
    {
        for (const auto& t : tasks)
        {
            std::cout << t.Name() << "\n";
        }
    }

    void Run()
    {
    }

protected:
    tf::Taskflow taskflow;
    tf::Executor executor;
};

void module()
{
    TaskModule t1("t1"), t2("t2"), t3("t3"), t4("t4"), t5("t5"), t6("t6");

    t1.Cond({t2, t3, t4}) >> t5 >> t6;

    t2.Before({ t4, t5 }) >> t3;

    t3 >> t4;

    TaskExecutor exec({t1, t2, t3, t4, t5, t6});

    exec.Run();
}

int main() {
    module();
    return 0;

    tf::Taskflow taskflow;
    tf::Executor executor;
    std::atomic<int> counter(0);

    auto [ a0, a1, a2, a3, a4, a5, a6, a7 ] = taskflow.emplace(
            [&] { std::cout << "a0\n"; },
            [&] { std::cout << "a1\n"; },
            [&] { std::cout << "a2\n"; },
            [&] { std::cout << "a3\n"; },
            [&] { std::cout << "a4\n"; },
            [&] { std::cout << "a5\n"; },
            [&] { std::cout << "a6\n"; },
            [&] { std::cout << "a7\n"; }
    );

    a0.name("0");
    a1.name("1");
    a2.name("2");
    a3.name("3");
    a4.name("4");
    a5.name("5");
    a6.name("6");
    a7.name("7");

    //a0 >> a1 >> a2.cond(a3, a4) >> a5;
    //a0.out("") >> a2;

    a0.precede(a4,a6,a7);

    a5.succeed(a1, a2, a3);
    a7.succeed(a3, a5);

    executor.run(taskflow).get();  // block until finished
    taskflow.dump(std::cout);

    return 0;
}
