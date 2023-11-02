#include <iostream>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <taskflow/taskflow.hpp>
#include <unordered_map>

#include <initializer_list>
#include <cstdarg>
#include <list>
#include <typeinfo>

enum TaskPrio
{
    HI = 0,
    NO = 1,
    LO = 2,
};

class TaskModule
{
public:
    TaskModule() : m_priority(TaskPrio::NO)
    {
    }

    virtual const std::string Name() const = 0;
    virtual bool IsCondition() const = 0;
    virtual uint32_t Run() = 0;

    void SetPriority(TaskPrio prio)
    {
        m_priority = prio;
    }

    TaskPrio GetPriority()
    {
        return m_priority;
    }

    TaskModule& operator >> (TaskModule* task)
    {
        //std::cout << "TaskModule: " << Name() << " >> " << task->Name() << "\n";
		m_lst.push_back(task);
        return *task;
    }

    TaskModule& Before(const std::initializer_list<TaskModule*>& tasks)
    {
        for (auto& t : tasks)
        {
            //std::cout << "TaskModule: " << Name() << " Before >> " << t->Name() << "\n";
            m_lst.push_back(t);
        }

        return *this;
    }

    std::list<TaskModule*>& GetList()
    {
        return m_lst;
    }

protected:
    TaskPrio m_priority;
    std::list<TaskModule*> m_lst;
};

template<typename TRunner>
class TModule : public TaskModule
{
public:
    const std::string Name() const override
    {
        return m_Runner.Name();
    }

    bool IsCondition() const override
	{
        return m_Runner.IsCondition();
    }

    uint32_t Run() override
    {
        return m_Runner.Run();
    }

public:
    TRunner m_Runner;
};

class TaskExecutor
{
public:
    TaskExecutor(const std::string& strName, const std::initializer_list<TaskModule*>& tasks): m_taskflow(strName)
    {
        for (auto& t : tasks)
        {
            tf::Task* curr = FindTask(t);

            if(nullptr == curr)
            {
                curr = AddTask(t);
            }

            std::list<TaskModule*>& lst = t->GetList();

            for (auto& lt : lst)
            {
                tf::Task* next = FindTask(lt);

                if(nullptr == next)
                {
                    next = AddTask(lt);
                }

                (*curr).precede(*next);
            }
        }
    }

    void Run(uint32_t nWorkers = 0)
    {
        if(nWorkers == 0)
        {
            nWorkers = std::thread::hardware_concurrency();
        }

        tf::Executor executor(nWorkers);
        executor.run(m_taskflow).get();
		m_taskflow.dump(std::cout);
    }

protected:

    tf::Task* FindTask(TaskModule* pMod)
    {
        auto it = m_mapTasks.find(pMod);
        if(it != m_mapTasks.end())
        {
            return &it->second;
        }
        return nullptr;
    }

    tf::Task* AddTask(TaskModule* pMod)
    {
        if(pMod->IsCondition())
        {
            m_mapTasks[pMod] = m_taskflow.emplace([=]() { return pMod->Run(); });
        }
        else
        {
            m_mapTasks[pMod] = m_taskflow.emplace([=]() { (void)pMod->Run(); });
        }

        tf::Task* curr = &m_mapTasks[pMod];
        curr->name(pMod->Name());
        
        switch(pMod->GetPriority())
        {
            case TaskPrio::HI:
                curr->priority(tf::TaskPriority::HIGH);
                break;
            case TaskPrio::NO:
                curr->priority(tf::TaskPriority::NORMAL);
                break;
            case TaskPrio::LO:
                curr->priority(tf::TaskPriority::LOW);
                break;
        }

        return curr;
    }

protected:
    std::unordered_map<TaskModule*, tf::Task> m_mapTasks;
    tf::Taskflow m_taskflow;
};

class InitGlobal
{
public:
    const std::string Name() const
    {
        return typeid(*this).name();
    }

    bool IsCondition() const
    {
        return false;
    }

    uint32_t Run()
    {
        std::cout << "Run class: " << Name() << "\n";
        return 0;
    }
};

class StartZmqSvr
{
public:
    const std::string Name() const
    {
        return typeid(*this).name();
    }

    bool IsCondition() const
    {
        return false;
    }

    uint32_t Run()
    {
        std::cout << "Run class: " << Name() << "\n";
        return 0;
    }
};

class ChkActState
{
public:
    const std::string Name() const
    {
        return typeid(*this).name();
    }

    bool IsCondition() const
    {
        return false;
    }

    uint32_t Run()
    {
        std::cout << "Run class: " << Name() << "\n";
        return 0;
    }
};

class ChkOtaEvt
{
public:
    const std::string Name() const
    {
        return typeid(*this).name();
    }

    bool IsCondition() const
    {
        return true;
    }

    uint32_t Run()
    {
        std::cout << "Run class: " << Name() << "\n";
        return 0;
    }
};

class Flash
{
public:
    const std::string Name() const
    {
        return typeid(*this).name();
    }

    bool IsCondition() const
    {
        return false;
    }

    uint32_t Run()
    {
        std::cout << "Run class: " << Name() << "\n";
        return 0;
    }
};

class Activate
{
public:
    const std::string Name() const
    {
        return typeid(*this).name();
    }

    bool IsCondition() const
    {
        return false;
    }

    uint32_t Run()
    {
        std::cout << "Run class: " << Name() << "\n";
        return 0;
    }
};

class EndFlash
{
public:
    const std::string Name() const
    {
        return typeid(*this).name();
    }

    bool IsCondition() const
    {
        return true;
    }

    uint32_t Run()
    {
        std::cout << "Run class: " << Name() << "\n";
        return 1;
    }
};

class EndAct
{
public:
    const std::string Name() const
    {
        return typeid(*this).name();
    }

    bool IsCondition() const
    {
        return true;
    }

    uint32_t Run()
    {
        std::cout << "Run class: " << Name() << "\n";
        return 0;
    }
};

class Reboot
{
public:
    const std::string Name() const
    {
        return typeid(*this).name();
    }

    bool IsCondition() const
    {
        return false;
    }

    uint32_t Run()
    {
        std::cout << "Run class: " << Name() << "\n";
        return 0;
    }
};

void module()
{
    TModule<InitGlobal>  modInitGlobal;
    TModule<StartZmqSvr> modStartZmqSvr;
    TModule<ChkActState> modChkActState;
    TModule<ChkOtaEvt>   modChkOtaEvt;
    TModule<Flash>       modFlash;
    TModule<Activate>    modActivate;
    TModule<EndFlash>    modEndFlash;
    TModule<EndAct>      modEndAct;
    TModule<Reboot>      modReboot;

    modInitGlobal.Before({&modStartZmqSvr, &modChkActState});
	modChkActState >> &modChkOtaEvt.Before({&modFlash, &modActivate});
    modActivate >> &modEndAct.Before({&modReboot, &modChkOtaEvt});
    modFlash >> &modEndFlash >> &modChkOtaEvt;

    modStartZmqSvr.SetPriority(TaskPrio::HI);

    TaskExecutor exec("OTA", {
            &modInitGlobal,
            &modStartZmqSvr,
            &modChkActState,
            &modChkOtaEvt,
            &modFlash,
            &modActivate,
            &modEndFlash,
            &modEndAct,
            &modReboot
    });

    exec.Run();
}

int main() {
    module();
/*
    tf::Executor executor(5);
    tf::Taskflow taskflow("test");

    printf("threads: %d\n", executor.num_workers());

    for(int i=0; i<5; i++) {
        taskflow.emplace([=](){
                printf("thread:%ld\n", pthread_self());
                std::this_thread::sleep_for(std::chrono::seconds(i));
                });
    }

    // submit the taskflow
    tf::Future fu = executor.run(taskflow);

    // request to cancel the submitted execution above
    //fu.cancel();

    // wait until the cancellation finishes
    fu.get();

    taskflow.dump(std::cout);
*/
    return 0;
}

