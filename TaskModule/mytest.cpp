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

class TaskModule
{
public:
    virtual const std::string Name() const = 0;
    virtual bool IsCondition() const = 0;
    virtual uint32_t Run() = 0;

    TaskModule& operator >> (TaskModule* task)
    {
        std::cout << "TaskModule: " << Name() << " >> " << task->Name() << "\n";
		m_lst.push_back(task);
        return *task;
    }

    TaskModule& Before(const std::initializer_list<TaskModule*>& tasks)
    {
        for (auto& t : tasks)
        {
            std::cout << "TaskModule: " << Name() << " Before >> " << t->Name() << "\n";
            m_lst.push_back(t);
        }

        return *this;
    }

    std::list<TaskModule*>& GetList()
    {
        return m_lst;
    }

protected:
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
    TaskExecutor(const std::initializer_list<TaskModule*>& tasks)
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

    void Run()
    {
		m_executor.run(m_taskflow).get();
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
        return curr;
    }

protected:
    std::unordered_map<TaskModule*, tf::Task> m_mapTasks;
    tf::Taskflow m_taskflow;
    tf::Executor m_executor;
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
    
    modInitGlobal.Before({&modChkActState, &modStartZmqSvr});
	modChkActState >> &modChkOtaEvt.Before({&modFlash, &modActivate});
    modActivate >> &modEndAct.Before({&modReboot, &modChkOtaEvt});
    modFlash >> &modEndFlash >> &modChkOtaEvt;

    printf("ini: %p\n", &modInitGlobal);

    TaskExecutor exec({
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
    return 0;
}
