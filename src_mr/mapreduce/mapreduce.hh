#ifndef _MAPREDUCE_
#define _MAPREDUCE_

enum role
{
	MASTER,
	MAP,
	REDUCE
};

static role status;

void init_mapreduce() // initialize mapreduce configure
{
	//TODO: determine status
	status = MASTER;
}

void summ_mapreduce() // summarize mapreduce configure
{
	//TODO: make sure that all configuration are done

	//TODO: manage all things if the status is master
}

void set_mapper(void (*map_func) (void))
{
	
}

void set_reducer(void (*red_func) (void))
{
	
}

void set_splitter(void (*split_func) (void))
{
	
}

void word_splitter(void)
{
	
}

void byte_splitter(void)
{
	
}

void sentence_splitter(void)
{
	
}


#endif
