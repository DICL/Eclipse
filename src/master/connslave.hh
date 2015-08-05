#ifndef __CONNSLAVE__
#define __CONNSLAVE__

#include <iostream>
#include <common/ecfs.hh>
#include "master_task.hh"

class connslave   // connection to the slave
{
    private:
		int rank
        int maxmaptask;
        int maxreducetask;
        vector<master_task*> running_tasks;
        
    public:
        connslave (int rank);
        connslave (int maxtask, int rank);
        ~connslave();
        int getrank();
        int getmaxmaptask();
        int getmaxreducetask();
        void setmaxmaptask (int num);
        void setmaxreducetask (int num);
        int getnumrunningtasks();
        master_task* getrunningtask (int index);
        void add_runningtask (master_task* atask);
        void remove_runningtask (master_task* atask);
};

connslave::connslave (int rank)
{
    this->maxmaptask = 0;
    this->maxreducetask = 0;
    this->rank = rank;
}

connslave::connslave (int maxtask, int rank)
{
    this->maxmaptask = maxtask;
    this->maxreducetask = maxtask;
    this->rank = rank;
}

connslave::~connslave()
{
}

int connslave::getrank()
{
    return this->rank;
}

int connslave::getmaxmaptask()
{
    return this->maxmaptask;
}

int connslave::getmaxreducetask()
{
    return this->maxreducetask;
}

int connslave::getnumrunningtasks()
{
    return this->running_tasks.size();
}

void connslave::setmaxmaptask (int num)
{
    this->maxmaptask = num;
}

void connslave::setmaxreducetask (int num)
{
    this->maxreducetask = num;
}

master_task* connslave::getrunningtask (int index)
{
    if ( (unsigned) index >= this->running_tasks.size())
    {
        cout << "Index out of bound in the connslave::getrunningtask() function." << endl;
        return NULL;
    }
    else
    {
        return this->running_tasks[index];
    }
}

void connslave::add_runningtask (master_task* atask)
{
    if (atask != NULL)
    {
        running_tasks.push_back (atask);
    }
    else
    {
        cout << "A NULL task is assigned to the running task vector in the connslave::add_runningtask() function." << endl;
    }
}

void connslave::remove_runningtask (master_task* atask)
{
    for (int i = 0; (unsigned) i < running_tasks.size(); i++)
    {
        if (running_tasks[i] == atask)
        {
            running_tasks.erase (running_tasks.begin() + i);
            break;
        }
    }
}

#endif
