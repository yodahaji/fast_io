#include"../../include/fast_io.h"
#include<sstream>

int main()
try
{
	std::stringstream sst("2353256");
	fast_io::stream_view sst_view(sst);
	std::size_t v(0);
	scan(sst_view,v);
	println(fast_io::out,v);
}
catch(std::exception const & e)
{
	println(fast_io::err,e);
	return 1;
}