﻿#include "iostream"
#include "string"
#include <sstream>
#include <fstream>
#include <regex>

#define CURL_STATICLIB
#include "curl/curl.h"
#include <nlohmann/json.hpp>

#pragma warning(disable: 4996)


using namespace nlohmann;


#ifdef _DEBUG
#	pragma comment (lib, "curl/libcurl_a_debug.lib")
#else
#	pragma comment (lib, "curl/libcurl_a.lib")
#endif


//
//  libcurl write callback function
//

static int writer(char *data, size_t size, size_t nmemb, std::string *writerData)
{
	if (writerData == NULL)
		return 0;

	writerData->append(data, size*nmemb);

	return size * nmemb;
}

/*

Tells what the problem is if there is one.

Returns if we should continue and look for transportation data or just quit.

*/

bool statusExplanation(int statusCode) {

	bool cont = true;

	switch (statusCode) {
	case 1001:
		std::cout << "problem with request : Key is undefined";
		cont = false;
		break;
	case 1002:
		std::cout << "problem with request : Key is invalid";
		cont = false;
		break;
	case 1003:
		std::cout << "Invalid api";
		cont = false;
		break;
	case 1004:
		std::cout << "problem with request : This api is currently not available for keys with priority above 2";
		cont = false;
		break;
	case 1005:
		std::cout << "problem with request : Invalid api for key";
		cont = false;
		break;
	case 1006:
		std::cout << "To many requests per minute";
		cont = false;
		break;
	case 1007:
		std::cout << "To many requests per month";
		cont = false;
		break;
	case 4001:
		std::cout << "SiteId m�ste gå att konvertera till heltal.";
		cont = false;
		break;
	case 5321:
		std::cout << "Kunde varken h�mta information fr�n TPI(tunnelbanan) eller DPS(�vriga trafikslag).";
		cont = true;
		break;
	case 5322:
		std::cout << "Kunde inte h�mta information fr�n DPS.";
		cont = true;
		break;
	case 5323:
		std::cout << "Kunde inte h�mta information fr�n TPI.";
		cont = true;
		break;
	case 5324:
		std::cout << "Kunde varken h�mta information fr�n TPI(tunnelbanan) eller DPS(�vriga trafikslag) p.g.a.inaktuell DPS - data.Detta uppst�r om DPS - datan �r �ldre �n 2 minuter vid svarstillf�llet.";
		cont = false;
		break;
	}
	return cont;
}



/*

string to nlohmann json object

*/
json toJson(const std::string jsonString) {
	json jsonObj;
	std::stringstream(jsonString) >> jsonObj;

	return jsonObj;

}

namespace jsTime {

	struct time {
		std::string week_number;
		std::string	utc_offset;
		std::string unixtime;
		std::string	timezone;
		std::string	dst_until;
		std::string	dst_from;
		bool dst;
		int day_of_year;
		int	day_of_week;
		std::string	datetime;
		std::string abbreviation;
	};

	void to_json(json& j, const time& t) {
		j = json{
			{"week_number", t.week_number},
			{"utc_offset", t.utc_offset},
			{"unixtime", t.unixtime},
			{"timezone", t.timezone},
			{"dst_until", t.dst_until},
			{"dst_from", t.dst_from},
			{"dst", t.dst},
			{"day_of_year", t.day_of_year},
			{"day_of_week", t.day_of_week},
			{"datetime", t.datetime},
			{"abbreviation", t.abbreviation}
		};
	}

	void from_json(const json& j, time& t) {
		j.at("week_number").get_to(t.week_number);
		j.at("utc_offset").get_to(t.utc_offset);
		j.at("unixtime").get_to(t.unixtime);
		j.at("timezone").get_to(t.timezone);
		j.at("dst_until").get_to(t.dst_until);
		j.at("dst_from").get_to(t.dst_from);
		j.at("dst").get_to(t.dst);
		j.at("day_of_year").get_to(t.day_of_year);
		j.at("day_of_week").get_to(t.day_of_week);
		j.at("datetime").get_to(t.datetime);
		j.at("abbreviation").get_to(t.abbreviation);
	}	
}



void printTransport (json::iterator transport, json &SLjson){
	if (transport != SLjson["ResponseData"].end()) {
		for (auto& t: json::iterator_wrapper(*transport)) {

			std::cout << "Line: ";
			std::cout << t.value()["LineNumber"].get<std::string>();

			std::cout << " ";

			std::cout << t.value()["TransportMode"].get<std::string>();

			std::cout << " toward ";
			std::cout << t.value()["Destination"].get<std::string>();

			std::cout << " departing ";

			std::cout << t.value()["DisplayTime"].get<std::string>();

			std::cout << ".";
			for (auto& d : json::iterator_wrapper(t.value()["Deviations"])) {
				std::cout << d.value()["Consequence"];
				std::cout << d.value()["Text"];
				std::cout << std::endl;
			}
			std::cout << std::endl;
		}
	}
}

bool getStringDataFromURL(std::string* dataToBeReceived, std::string URL) {

	const char* URLcstr = URL.c_str();

	CURL *curl = nullptr;
	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, URLcstr);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, dataToBeReceived);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);

		CURLcode code = curl_easy_perform(curl);

		curl_easy_cleanup(curl);

		if (code == CURLE_OK) {
			return true;
		}
	}


	dataToBeReceived->clear();
	return false;
}



bool getLocation(json* locationjson, std::string locationQ, std::string APIKEY) {
	std::string content;

	std::string URL = "https://eu1.locationiq.com/v1/search.php?key=" + APIKEY + "&q=" + locationQ + "&limit=2&format=json";

	if (getStringDataFromURL(&content, URL)) {
		*locationjson = toJson(content);

		// iterator is not at the end, which means that error was found.
		// an error key means that we did not in fact get location.
		// Therefore we return the inverse of if we found an error,
		// such that false is error, and true is no error.
		return !(locationjson->find("error") != locationjson->end());
	}

	return false;
}

bool getWeather(json* weatherjson, std::string lat, std::string lng, std::string APIKEY) {
	std::string content;
	std::string URL = "http://api.openweathermap.org/data/2.5/weather?lat=" + lat + "&lon=" + lng + "&APPID=" + APIKEY;
	if (getStringDataFromURL(&content, URL)) {
		*weatherjson = toJson(content);
		return true;
	}

	return false;
}

bool getNearbyStations(json* nearbytrimmed, std::string lat, std::string lng, int maxNo, int radius, std::string APIKEY) {
	std::string content;

	std::string URL = "api.sl.se/api2/nearbystopsv2.json?key=" + APIKEY + "&originCoordLat=" + lat + "&originCoordLong=" + lng + "&maxNo=" + std::to_string(maxNo) + "&r" + std::to_string(radius);

	if (getStringDataFromURL(&content, URL)) {

		json nearbyjson;
		nearbyjson = toJson(content);

		

		std::cout << nearbyjson.dump(4);

		auto responseDataPtr = nearbyjson.find("stopLocationOrCoordLocation");
		if (responseDataPtr != nearbyjson.end()){
			for (json::iterator it = nearbyjson["stopLocationOrCoordLocation"].begin(); it != nearbyjson["stopLocationOrCoordLocation"].end(); ++it) {
				auto obj = it.value().find("StopLocation");
				if (obj != it.value().end()) {
					nearbytrimmed->push_back(it.value());
				}
			}

			return true;
		}
		else {
			return false;
		}
		//json::iterator nearbyStations;
	}

	return false;
}


bool getDepartures(json* SLjson, int siteID, int timeWindow, std::string APIKEY) {

	if (timeWindow < 0 || timeWindow > 60) {
		std::cout << "timeWindow less than 0 or more than 60. 60 is upper limit, <0 doesnt work with SL realtimedeparturesV4. " << std::endl;
		return false;
	}

	std::string siteIDstr = std::to_string(siteID);
	std::string timeWindowstr = std::to_string(siteID);
	std::string URL = "http://api.sl.se/api2/realtimedeparturesV4.JSON?key=" + APIKEY + "&siteid=" + siteIDstr + "&timewindow=" + timeWindowstr;

	std::string content;

	if (getStringDataFromURL(&content, URL)) {

		*SLjson = toJson(content);

		auto SC = SLjson->find("StatusCode");

		if (SC != SLjson->end()) {
			int statusCode = SC.value().get<int>();
			return statusExplanation(statusCode);
		}
		else {
			return true;
		}
	}

	return false;
}

bool getTime(json* timeJson) {

	std::string URL = "http://worldtimeapi.org/api/timezone/Europe/Stockholm";
	std::string content;

	if (getStringDataFromURL(&content, URL)) {
		*timeJson = toJson(content);
		return true;
	}
	return false;
}


std::string getFileContents (std::string fileName){

	std::ifstream ifs(fileName);
	std::string content((std::istreambuf_iterator<char>(ifs)),
		(std::istreambuf_iterator<char>()));

	return content;
}

// Todo: make datetime format into simple clock format, hh:mm:ss:ms

std::string timeStrFromDatetime(std::string datetime) {
	return "10:00:00";
}

char getInputChar(char* viableChars, int size) {
	char input;
	bool legit = false;
	do {
		input = std::cin.get();
		for (int i = 0; i < size; i++) {
			if (viableChars[i] == input) {
				legit = true;
			}
		}
	} while (legit == false);

	return input;
}

void printTrimmedStations(json* stations) {
	for (auto& station : json::iterator_wrapper(*stations)) {
		
		std::cout << "Hållplats: ";
		std::cout << station.value()["Name"].get<std::string>();
		std::cout << ", avstånd: ";
		std::cout << station.value()["Dist"].get<std::string>();
		std::cout << "m";
		std::cout << std::endl;
	}
}

int wmain(int argc, wchar_t *argv[]) {

	curl_global_init(CURL_GLOBAL_ALL);

	json APIKEYS = toJson(getFileContents("APIKEYS.txt"));

	json timejson;
	json SLjson;
	json locationjson;
	json weatherjson;
	json nearbyjson;
	json nearbytrimmed;

	bool gotDepartures			= false;
	bool gotTime				= false;
	bool gotLocation			= false;
	bool gotWeather				= false;
	bool gotNearbyStations		= false;
	bool gotSiteID				= false;

	int siteID = 9192;
	int timeWindowMinutes = 60;
	int nearbyStationMax = 9;		// 9 stations
	int nearbyStationRadius = 2000;	// 2000 meter radius

	std::string locationQ;
	std::string lat;
	std::string lon;

	std::cout << "Enter an address you'd like to stuff for: ";
	std::cin >> locationQ;

	gotLocation = getLocation(&locationjson, locationQ, APIKEYS["locationiq"].get<std::string>());

	/*

	Flow:
		User enters address or place near you.
		json narliggande = Närliggande hållplatser API anrop
		Sedan får användaren välja vilken närliggande hållplats som är "Best match"
		siteID = platsuppslag av best match.
		Alternativt, om vi hittat en hållplats med potentiellt tunnelbana, visa den som ett "extra bra" alternativ.

*/


	if (gotLocation) {

		auto it = locationjson.begin();
		lat = it.value()["lat"].get<std::string>();
		lon = it.value()["lon"].get<std::string>();

		std::cout << lat << " " << lon << std::endl;
		gotWeather = getWeather(&weatherjson, lat, lon, APIKEYS["openweather"].get<std::string>());

		if (gotWeather) {
			std::cout << "temperature: ";
			std::cout << weatherjson["main"]["temp"].get<float>();
			std::cout << " K" << std::endl;
		}

		gotNearbyStations = getNearbyStations(&nearbytrimmed, lat, lon, nearbyStationMax, nearbyStationRadius, APIKEYS["nearby"]);
	}

	std::cout << std::endl << std::endl;
	
	if (gotNearbyStations) {
		std::cout << nearbytrimmed.dump(4);
		for (json::iterator station = nearbytrimmed.begin(); station != nearbytrimmed.end(); ++station) {
			std::cout << "Station ";
			std::cout << station.value()["StopLocation"]["name"].get<std::string>();
			std::cout << std::endl;
		}
	}





	/*
	gotDepartures = getDepartures(&SLjson, siteID, timeWindowMinutes, APIKEYS["realtimedepartures"].get<std::string>());

	if ( gotDepartures ) {

		auto BusesPtr = SLjson["ResponseData"].find("Buses");
		auto MetrosPtr = SLjson["ResponseData"].find("Metros");
		auto TrainsPtr = SLjson["ResponseData"].find("Trains");
		auto TramsPtr = SLjson["ResponseData"].find("Trams");
		auto ShipsPtr = SLjson["ResponseData"].find("Ships");

		printTransport(BusesPtr, SLjson);
		printTransport(MetrosPtr, SLjson);
		printTransport(TrainsPtr, SLjson);
		printTransport(TramsPtr, SLjson);
		printTransport(ShipsPtr, SLjson);

	}

	gotTime = getTime(&timejson);

	std::cout << std::endl;

	if ( gotTime ) {
		std::cout << timejson["datetime"].get<std::string>();
	}
	*/
	curl_global_cleanup();

	std::cin.get();
	return 0;
}