#ifndef __IWFREQUEST__
#define __IWFREQUEST__

class iwfrequest
{
    private:
        int received;
        int requested;
        int jobid;
        
    public:
        vector<int> peerids;
        vector<int*> numblocks;
        
        int get_jobid();
        bool is_finished();
        iwfrequest (int ajobid);
        void add_request (int num);
        void add_receive (int index, int *num_block);
		~iwfrequest();
};

iwfrequest::iwfrequest (int ajobid)
{
    jobid = ajobid;
    received = 0;
    requested = 0;
}

iwfrequest::~iwfrequest() {
	for (int i = 0; (unsigned) i < numblocks.size(); ++i)
		delete[] numblocks[i];
}

void iwfrequest::add_request (int num)
{
    peerids.push_back (num);
    numblocks.push_back (NULL);
    requested++;
}

int iwfrequest::get_jobid()
{
    return jobid;
}

void iwfrequest::add_receive (int index, int *num_block)
{
    for (int i = 0; (unsigned) i < peerids.size(); i++)
    {
        if (peerids[i] == index)
        {
            numblocks[i] = num_block;
            received++;
            return;
        }
    }
    
    cout << "check add_receive()" << endl;
}

bool iwfrequest::is_finished()
{
    if (received == requested)
    {
        return true;
    }
    else
    {
        return false;
    }
}

#endif
