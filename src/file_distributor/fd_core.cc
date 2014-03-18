#include <iostream>
#include <common/hash.hh>
#include <fstream>
#include <sstream>
#include <mapreduce/definitions.hh>

using namespace std;

int main(int argc, char** argv)
{
	// initialize data structures from setup.conf
	ifstream conf;
	int num_slave;
	string token;
	string confpath = LIB_PATH;
	confpath.append("setup.conf");
	conf.open(confpath.c_str());

	conf>>token;
	while(!conf.eof())
	{
		if(token == "port")
		{
			// ignore and just pass through this case
			conf>>token;
		}
		else if(token == "dhtport")
		{
			// ignore and just pass through this case
			conf>>token;
		}
		else if(token == "max_job")
		{
			// ignore and just pass through this case
			conf>>token;
		}
		else if(token == "num_slave")
		{
			// ignore and just pass through this case
			conf>>token;
			num_slave = atoi(token.c_str());
			break;
		}
		else if(token == "master_address")
		{
			// ignore and just pass through this case
			conf>>token;
		}
		else
		{
			cout<<"[master]Unknown configure record: "<<token<<endl;
		}
		conf>>token;
	}
	conf.close();

	// argv[1] name of input file which includes list of input files 
	// argv[2] name of output file which will match the input files and target address
	string inputfilename = MR_PATH;
	string outputfilename = MR_PATH;
	inputfilename.append(argv[1]);
	outputfilename.append(argv[2]);

	string filename;
	ifstream input;
	ofstream output;
	input.open(inputfilename.c_str());
	output.open(outputfilename.c_str());

	while(1)
	{
		getline(input, filename);
		if(input.eof())
		{
			break;
		}
		else
		{
			string address;
			stringstream ss;
			uint32_t hashvalue = h(filename.c_str(), HASHLENGTH);
			ss<<(hashvalue%num_slave)+1;
			address = ADDRESSPREFIX;
			address.append(ss.str());
			output<<"scp "<<MR_PATH<<filename<<" "<<address<<":"<<DHT_PATH<<endl;
			//output<<filename<<" "<<address<<endl;
		}
	}
}
