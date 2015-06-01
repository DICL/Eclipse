#include "ecfs.hh"

msgaggregator::msgaggregator()
{
    // set fd and pos
    fd = -1;
    pos = 0;
    initial = "";
    msgbuf = NULL;
    dwriter = NULL;
}

msgaggregator::msgaggregator (int number)
{
    // set fd and pos
    fd = number;
    pos = 0;
    initial = "";
    msgbuf = NULL;
    dwriter = NULL;
}

int msgaggregator::get_available()
{
    return BUF_SIZE - pos - 2; // -1 for null character and another -1 for newline character
}

bool msgaggregator::add_record (string& record)     // return true when flushed
{
    // check whether the new record can be added
    if ( (unsigned) get_available() > record.length())      // new record can be added
    {
        // add newline character at the end of current message
        if ( (unsigned) pos != initial.length())
        {
            message[pos] = '\n';
            pos++;
        }
        
        // append the content(record)
        strcpy (message + pos, record.c_str());
        pos += record.length();
        
        // check if the current buffer size exceeds threshold
        if (pos > BUF_THRESHOLD)
        {
            flush();
            return true;
        }
        else
        {
            return false;
        }
    }
    else     // new record should be added to next buffer and current buffer should be flushed
    {
        flush();
        //message[pos] = '\n';
        //pos++;
        // append the content(record)
        strcpy (message + pos, record.c_str());
        pos += record.length();
        
        // check if the current buffer size exceeds threshold
        if (pos > BUF_THRESHOLD)
        {
            flush();
        }
        
        return true;
    }
}

bool msgaggregator::add_record (char*& record)     // <- automatically flushed??
{
    string input = record;
    return add_record (input);
}

void msgaggregator::configure_initial (string record)     // the white space should be explicitly added to the parameter string
{
    initial = record;
    set_initial();
}

void msgaggregator::set_initial()
{
    memset (message, 0, BUF_SIZE);
    strcpy (message, initial.c_str());
    pos = initial.length();
}

void msgaggregator::set_msgbuf (vector<messagebuffer*>* target)
{
    msgbuf = target;
}

void msgaggregator::flush()   // return false when new messagebuffer is needed to be created
{
    // do not flush when message have no information
    if ( (unsigned) pos == initial.length())
    {
        return;
    }
    
    // write to the cache if writing is ongoing
    if (dwriter != NULL)
    {
        dwriter->write_record (message);
    }
    
    // write to the fd
    if (msgbuf == NULL)     // no target messagebuf(client side)
    {
//cout<<"flushed message: "<<message<<endl<<endl;
        nbwrite (fd, message);
        // set initial contents
        set_initial();
        return;
    }
    else     // write to target messagebuf(fileserver side)
    {
        if (msgbuf->size() > 1)
        {
            //cout<<"flushed message: "<<message<<endl<<endl;
            msgbuf->back()->set_buffer (message, fd);
            msgbuf->push_back (new messagebuffer());
            // set initial contents
            set_initial();
            return;
        }
        else
        {
//cout<<"flushed message: "<<message<<endl<<endl;
            if (nbwritebuf (fd, message, msgbuf->back()) <= 0)
            {
                // append new message buffer
                msgbuf->push_back (new messagebuffer());
                // set initial contents
                set_initial();
                return;
            }
            else
            {
                // set initial contents
                set_initial();
                return;
            }
        }
    }
}

void msgaggregator::set_fd (int num)
{
    fd = num;
}

int msgaggregator::get_fd()
{
    return fd;
}

char* msgaggregator::get_buf()
{
    return message;
}

void msgaggregator::set_dwriter (entrywriter* awriter)
{
    dwriter = awriter;
}
