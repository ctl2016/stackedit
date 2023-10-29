#include <iostream>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <taskflow/taskflow.hpp>
#include <unordered_map>

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

class TaskRunner {

public:

    virtual uint32_t Run()
    {
        std::cout << "Run(" << "m_strName" << ")\n";
        return 0;
    }
};

class TaskModule {

public:

    TaskModule(const std::string& strName, TaskRunner* runner, bool bCond = false):
        m_strName(strName),
        m_pRunner(runner),
        m_bCondition(bCond)
    {
    }

    const std::string& Name() const
    {
        return m_strName;
    }

    int32_t Run()
    {
        return m_pRunner->Run();
    }

    TaskModule& operator >> (TaskModule* task)
    {
        std::cout << m_strName << " >> " << task->Name() << "\n";
		m_lst.push_back(task);
        return *task;
    }

    TaskModule& Before(const std::initializer_list<TaskModule*>& tasks)
    {
        for (const auto t : tasks)
        {
            std::cout << m_strName << " Before >> " << t->Name() << "\n";
            m_lst.push_back(t);
        }

        return *this;
    }

    const std::list<const TaskModule*>& GetList() const
    {
        return m_lst;
    }

    bool IsCondition() const {
        return m_bCondition; 
    }

protected:
    std::string m_strName;
    TaskRunner* m_pRunner;
    bool m_bCondition;
    std::list<const TaskModule*> m_lst;
};

class TaskExecutor {

public:

    TaskExecutor(const std::initializer_list<TaskModule*>& tasks)
    {
        for (const auto& t : tasks)
        {
            tf::Task curr;

            if(!FindTask(t->Name(), curr))
            {
                if(t->IsCondition())
                {
                    curr = m_taskflow.emplace([tc = *t]() mutable { return tc.Run(); });
                }
                else
                {
                    curr = m_taskflow.emplace([tc = *t]() mutable { (void)tc.Run(); });
                }

                curr.name(t->Name());
                m_mapTasks[t->Name()] = curr;
            }

            std::cout << t->Name() << "\n";

            const std::list<const TaskModule*>& lst = t->GetList();

            for (const auto& lt : lst)
            {
				std::cout << lt->Name() << " lt \n";
                tf::Task next;

                if(!FindTask(lt->Name(), next))
                {
                    if(lt->IsCondition())
                    {
                        next = m_taskflow.emplace([tn = *lt]() mutable { return tn.Run(); });
                    }
                    else
                    {
                        next = m_taskflow.emplace([tn = *lt]() mutable { (void)tn.Run(); });
                    }
                    next.name(lt->Name());
                    m_mapTasks[lt->Name()] = next;
                }

				curr.precede(next);
            }
        }
    }

    void Run()
    {
		m_executor.run(m_taskflow).wait();
		m_taskflow.dump(std::cout);
    }

protected:

    bool FindTask(const std::string& strName, tf::Task& task)
    {
        auto it = m_mapTasks.find(strName);

        if(it != m_mapTasks.end())
        {
            task = it->second;
            return true;
        }

        return false;
    }

protected:
    std::unordered_map<std::string, tf::Task> m_mapTasks;
    tf::Taskflow m_taskflow;
    tf::Executor m_executor;
};

class TaskA : public TaskRunner
{
public:
    uint32_t Run()
    {
        std::cout << "TaskA::" << "\n";
        return 0;
    }
};

class TaskB : public TaskRunner
{
public:
    uint32_t Run()
    {
        std::cout << "TaskB::" << "\n";
        return 0;
    }
};

void module()
{
    TaskA a;
    TaskB b;
    TaskModule t1("t1", &a), t2("t2", &a), t4("t4", &a), t5("t5", &b), t6("t6", &b);
    TaskModule t3("t3", &b, true);
    t1.Before({&t2, &t3, &t4}) >> &t5 >> &t6;
    t3.Before({&t4, &t6});
    TaskExecutor exec({&t1, &t2, &t3, &t4, &t5, &t6});
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

    //a0.precede(a4,a6,a7);

    a0.precede(a3);

    a5.succeed(a1, a2, a3);
    a7.succeed(a3, a5);

    executor.run(taskflow).get();  // block until finished
    taskflow.dump(std::cout);

    return 0;
}

