#include <simring.hh>
#include <assert.h>

int main () {
	uint64_t success = 0;
	uint64_t failed = 0;

	SETcache cache (3, "input1.trash");

	//--------------------------------------//

	for (int i = 3; i < 7; i++) {

		if (cache.match (i, 1, 10)) 
			success++;
		else
			failed++;

		cout << cache << endl;
	}
	assert (failed == 4);

	//--------------------------------------//

	if (cache.match (8, 5, 10)) 
		success++;
	else
		failed++;

	cout << cache << endl;

 return 0;
}
