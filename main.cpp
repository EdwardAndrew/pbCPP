#include <string>
#include <sstream>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <chrono>
#include <vector>
#include <mutex>
#include <fstream>
#include <math.h>
#include <algorithm>

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

int RequestsMade = 0;
int ThreadCount = std::thread::hardware_concurrency();
std::vector<std::string> lines;
std::mutex mtx;
std::string url;

bool validateArgs(int argc, char* argv[])
{
	bool isValid = true;

	if(argc < 2) isValid = false;

	return true;
}

size_t WriteStringCallback(char* ptr, size_t size, size_t nmemb)
{
	std::string data = std::string(ptr, size*nmemb);
    int totalSize= size*nmemb;
	std::cout << "DATA:  " << data << std::endl;
    return totalSize;
}

void thread_function()
{
	cURLpp::Easy handle;
	std::ostringstream os;
	cURLpp::options::WriteStream ws(&os);
	handle.setOpt(ws);
	curlpp::options::WriteFunction wf(WriteStringCallback);
	handle.setOpt(wf);
	std::list<std::string> header;
	header.push_back("Content-Type: application/json");
	std::string cookies = "";
	std::string body = "{\"id\": \"beep\"}";

	while(!lines.empty()){
		mtx.lock();
		std::string password = lines.back();
		lines.pop_back();
		RequestsMade++;
		mtx.unlock();
		handle.setOpt<cURLpp::options::Url>(url);
		handle.setOpt<cURLpp::options::HttpHeader>(header);
		handle.setOpt<cURLpp::options::CookieList>(cookies);
		handle.setOpt<cURLpp::options::PostFields>(body);
		handle.setOpt<cURLpp::options::PostFieldSize>(body.length());
		handle.setOpt<cURLpp::options::UserAgent>("");
		handle.perform();
		os.clear();
	}
}


int main(int argc, char* argv[])
{
	// if(!validateArgs(argc, argv)) return 1;
	url = argv[1];
	std::string wordlistPath = argv[2];
	ThreadCount = std::stoi(argv[3]);

	std::ifstream wordlist;
	wordlist.open(wordlistPath);
	std::string line;
	std::cout << "Reading " << wordlistPath << std::endl;

	while(std::getline(wordlist, line))
	{
		lines.push_back(line);
	}
	wordlist.close();
	std::random_shuffle(lines.begin(), lines.end());

	std::cout << lines.size() << " passwords read and shuffled." << std::endl;
	std::cout << ThreadCount << " threads" << std::endl;

    cURLpp::initialize();

	auto start = std::chrono::system_clock::now();

	std::vector<std::thread> pool(ThreadCount);
	for(int i =0; i < ThreadCount; i++)
	{
		pool.at(i) = std::thread(thread_function);
		pool.at(i).detach();
	}

	sleep(1);
	int previousRequests = 0;
	while(!lines.empty()){
		int requestsPerSecond =  RequestsMade - previousRequests;
		double secondsRemaining =  (lines.size()-RequestsMade) / requestsPerSecond;

		int hours = floor((secondsRemaining/60)/60);
		int minutes = (int)(floor(secondsRemaining / 60)) % 60;
		int seconds = (int)secondsRemaining % 60; 

		std::string eta = "";
		if(hours < 10) eta.append("0");
		eta.append(std::to_string(hours));
		eta.append(":");
		if(minutes < 10) eta.append("0");
		eta.append(std::to_string(minutes));
		eta.append(":");
		if(seconds < 10) eta.append("0");
		eta.append(std::to_string(seconds));

		std::cout << "Requests p/s " << requestsPerSecond << "s" << " | " << RequestsMade << "/" << lines.size() << "| ETA: " << eta << "\r";
		std::cout.flush();
		previousRequests = RequestsMade;
		sleep(1);
	}

	auto end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed_seconds = end-start;
	std::cout << RequestsMade << " requests made in " << elapsed_seconds.count() << "s" << std::endl;

	cURLpp::terminate();

	return 0;
}