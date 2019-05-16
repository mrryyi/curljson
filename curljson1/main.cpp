#include "iostream"
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

Status explanation for departures.

Returns if we should continue and look for transportation data or just quit.

*/

bool departureStatusCodeExplanation(int statusCode) {

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
	Location
	
	Represents a place with a name and coordinates.
	
	Used to find nearby station names, and weather.
	
	The name is there to connect coordinates with something intelligible to the human.

*/

class Location {
public:
	Location(std::string name, std::string lat, std::string lon) :
		name(name), lat(lat), lon(lon) {};
	const std::string name;
	const std::string lat;
	const std::string lon;
};

/*
	Station

	A class that represents a station with a siteID and a name.

	A siteID is the number (as a string) that we call the platsuppslag API with.

*/

class Station {

	const std::string siteID;
	const std::string name;

public:
	Station(std::string siteID, std::string name) : siteID(siteID), name(name)
	{
	}

	std::string getSiteID() { return siteID; };
	std::string getName() { return name; };
};

/*

string to nlohmann json object

*/
json toJson(const std::string jsonString) {
	json jsonObj;
	std::stringstream(jsonString) >> jsonObj;

	return jsonObj;

}

/*

	Gets a json iterator that represents a type of transport departures.
	For example, METROS. Then we need to use that iterator to go inside METROS,
	which is a list of departures. From this we can get information about that departure,
	for example a list of deviations for just that departure. Line number. Destination. When it should depart.
*/

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

/*
	This is the bread and butter of getting our data.
	We use this function to get data from an API, and we save it in a 
	string that came as an argument and was accepted as a pointer.

	We make the URL into a cstr because libcurl loves const char arrays.

	We make a CURL object thing that is initialized to a nullptr.

	Then we attempt to initialize the curl object, and if so, we can begin
	setting the options.

	We want to clear the string if libcurl failed, because we may have appended stuff to it,
	and then suddenly something went wrong.
		We shall not return faulty, incomplete data.

*/

bool getStringDataFromURI(std::string* dataToBeReceived, std::string URI) {

	const char* URIcstr = URI.c_str();

	CURL *curl = nullptr;
	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, URIcstr);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, dataToBeReceived);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);

		CURLcode code = curl_easy_perform(curl);

		// WE CLEAN UP 
		curl_easy_cleanup(curl);

		if (code == CURLE_OK) {
			return true;
		}
	}

	dataToBeReceived->clear();
	return false;
}

/*

Gets locations by reading a location json from LocationIQ api by searching with param string.

The json is parsed into making a minimized version of the location objects such that we 
can handle them with more ease later. By that I mean indexing the fucking things.

As far as is known at the time of making this project, nlohmann json lists are not indexable.

Which is a con of the nlohmann library, contrary to javascript builtin json parsing. But the positive side is that we can create
our own representation, as a class, with blackjack and hookers.

Actually, maybe it is a good thing, because MAYBE, I do not know, javascript does a linear inefficient 
search to find each element in the json list when you write something like data.items[9].

Or they use a hash function, which would make it quick AF but require more memory.

Take and give youknow.

Location has lat lon name, all strings.

*/


bool getLocation(std::vector<Location>* locations, std::string locationQ, std::string APIKEY) {

	std::cout << "Contacting API: LocationIQ!" << std::endl;
	std::string content;

	std::string URI = "https://eu1.locationiq.com/v1/search.php?key=" + APIKEY + "&q=" + locationQ + "&limit=5&format=json";

	if (getStringDataFromURI(&content, URI)) {

		json locationjson;
		try
		{
			locationjson = toJson(content);
		}
		catch (json::parse_error& e)
		{
			std::cout << "json parsing error" << std::endl;
			std::cout << "message: " << e.what() << '\n'
				<< "exception id: " << e.id << '\n'
				<< "byte position of error: " << e.byte << std::endl;
			std::cout << std::endl;
			return false;
		}

		for (json::iterator loc = locationjson.begin(); loc != locationjson.end(); ++loc) {
			locations->push_back(
				Location(loc.value()["display_name"].get<std::string>(),
						 loc.value()["lat"].get<std::string>(),
						 loc.value()["lon"].get<std::string>()));
		}
		// iterator is not at the end, which means that error was found.
		// an error key means that we did not in fact get location.
		// Therefore we return the inverse of if we found an error,
		// such that false is error, and true is no error.
		return !(locationjson.find("error") != locationjson.end());
	}

	return false;
}

/*

We get the weather from openweather API. Not much to say here, we get it into a json file
because we don't really need a class for it as all we do is print the result.

Could be optimized by appending the info we want to print into a string pointer argument,
like so:
	"Weather: " + *weatherjson["weather"].begin().value()["main"].get<std::string>();
	...
	And so on for what we want.

	So the string gets what we print and then we just print the string.

	But it is not so, we handle these things down below in the menu thing.

*/

bool getWeather(json* weatherjson, std::string lat, std::string lng, std::string APIKEY) {
	std::cout << "Contacting API: Open Weather Map!" << std::endl;
	std::string content;
	std::string URI = "http://api.openweathermap.org/data/2.5/weather?lat=" + lat + "&lon=" + lng + "&APPID=" + APIKEY + "&units=metric";
	if (getStringDataFromURI(&content, URI)) {
		*weatherjson = toJson(content);
		return true;
	}

	return false;
}

/*

Here we get nearby stop names, that is to say, WE ONLY GET STRINGS OF STOPS IN THE AREA INDICATED BY PARAMETER COORDINATES.

These strings cannot be used to get departures directly, because we need a siteID.

WHY do these not contain any siteID's?

I do not know, but we work with what we got.

We only save it as a string in a vector. This is because all we really need or want to do with the information
is to lead to a station. So we don't care about miscellaneous information of the result. Just the name of the stop location,
because that's what we have to search with.

*/

bool getNearbyStopNames(std::vector<std::string>* nearbyStationNames, std::string lat, std::string lng, int maxNo, int radius, std::string APIKEY) {
	std::cout << "Contacting API: Nearby stops!" << std::endl;
	std::string content;

	std::string URI = "api.sl.se/api2/nearbystopsv2.json?key=" + APIKEY + "&originCoordLat=" + lat + "&originCoordLong=" + lng + "&maxNo=" + std::to_string(maxNo) + "&r" + std::to_string(radius);

	if (getStringDataFromURI(&content, URI)) {

		json nearbyjson;

		try
		{
			nearbyjson = toJson(content);
		}
		catch (json::parse_error& e)
		{
			std::cout << "json parsing error" << std::endl;
			std::cout << "message: " << e.what() << '\n'
				<< "exception id: " << e.id << '\n'
				<< "byte position of error: " << e.byte << std::endl;
			std::cout << std::endl;
			return false;
		}

		bool foundAStation = false;

		auto responseDataPtr = nearbyjson.find("stopLocationOrCoordLocation");
		if (responseDataPtr != nearbyjson.end()){
			for (json::iterator it = nearbyjson["stopLocationOrCoordLocation"].begin(); it != nearbyjson["stopLocationOrCoordLocation"].end(); ++it) {
				auto obj = it.value().find("StopLocation");
				if (obj != it.value().end()) {
					nearbyStationNames->push_back(it.value()["StopLocation"]["name"].get<std::string>());
					foundAStation = true;
				}
			}

			return foundAStation;
		}
	}

	// libcurl expressing request or response failure.
	return false;
}

/*

platsuppslag API

Using a chosen stop name that was got from getNearbyStopNames, we use that as a query to get a bunch of potential stations
that actually can be used to get departures, because these contain siteID's.

We append to a parameter vector of the Station class. The Station class just contains siteID and name, such that we can easily
print, index and choose in a menu of stations, like so:
	[0] st eiksplan, siteID 9117 or whatever
	[1] x station
	[2] y station
	enter a number: 

	etc.

	Making classes and using a vector to store them REALLY eases the choosing processes.

*/

bool getStationsFromStopName(std::vector<Station>* stations, std::string candidate, std::string APIKEY) {
	std::cout << "Contacting API: Platsuppslag!" << std::endl;
	std::string content;

	std::string URI = "https://api.sl.se/api2/typeahead.json?key=" + APIKEY + "&searchstring=" + candidate + "&stationsonly=true&maxresults=8";
	if (getStringDataFromURI(&content, URI)) {
		json sts;

		try
		{
			// parsing input with a syntax error
			sts = toJson(content);
		}
		catch (json::parse_error& e)
		{
			std::cout << "json parsing error" << std::endl;
			std::cout << "message: " << e.what() << '\n'
				<< "exception id: " << e.id << '\n'
				<< "byte position of error: " << e.byte << std::endl;
			std::cout << std::endl;
			return false;
		}

		std::string siteID;
		std::string name;

		auto check = sts.find("ResponseData");
		if (check != sts.end()) {
			// We append the 
			for (json::iterator station = sts["ResponseData"].begin(); station != sts["ResponseData"].end(); ++station) {
				siteID = station.value()["SiteId"].get<std::string>();
				name = station.value()["Name"].get<std::string>();
				stations->push_back(Station(siteID, name));
			}

			// If we find any
			return true;
		}
	}

	// libcurl expressing request or response failure.
	return false;
}

/*

We get departures and save it in a json that came in as a parameter.

We get the departures by using a siteID, which we got from platsuppslag.

There is status code handling here because there are instances where we
don't even get a "responsedata" json object. So therefore we quit if there is such a one.

This will only return true if there are some type of departures available.

*/

bool getDepartures(json* SLjson, std::string siteID, int timeWindow, std::string APIKEY) {

	if (timeWindow < 0 || timeWindow > 60) {
		std::cout << "timeWindow less than 0 or more than 60. 60 is upper limit, <0 doesnt work with SL realtimedeparturesV4.\nSetting timewindow to 60 min." << std::endl;
		timeWindow = 60;
	}

	std::string timeWindowstr = std::to_string(timeWindow);
	std::string URI = "http://api.sl.se/api2/realtimedeparturesV4.JSON?key=" + APIKEY + "&siteid=" + siteID + "&timewindow=" + timeWindowstr;

	std::string content;

	if (getStringDataFromURI(&content, URI)) {


		try
		{
			*SLjson = toJson(content);
		}
		catch (json::parse_error& e)
		{
			std::cout << "json parsing error" << std::endl;
			std::cout << "message: " << e.what() << '\n'
				<< "exception id: " << e.id << '\n'
				<< "byte position of error: " << e.byte << std::endl;
			std::cout << std::endl;
			return false;
		}

		auto SC = SLjson->find("StatusCode");

		if (SC != SLjson->end()) {
			int statusCode = SC.value().get<int>();
			return departureStatusCodeExplanation(statusCode);
		}
		else {
			return true;
		}
	}

	// libcurl expressing request or response failure.
	return false;
}

/*

We use this to read text files, or specifically just APIKEYS.txt

Returns the contents of the file, which in this project we convert into a nlohman object.

*/

std::string getFileContents (std::string fileName){

	std::ifstream ifs(fileName);
	std::string content((std::istreambuf_iterator<char>(ifs)),
		(std::istreambuf_iterator<char>()));

	return content;
}

/*
	getInputInt:

	gets user input within a given range. There is no quitting. Answer within the range or never escape.

*/

int getInputInt(int min, int max) {
	std::string input;
	int num;
	bool legit = false;
	do {
		std::cin >> input;
		// The "-" is in the inclusive string because we want -1 to be a choice.
		// Of course, this opens up to behaviour where a dash can be in a position other 
		// than the first, like 1-, 1- or 1-1, etc.. This is not good.
		if (input.find_first_not_of("-0123456789") == std::string::npos) {
			num = stoi(input);
			if (!(num < min || num > max))
				legit = true;
		}
	} while (!legit);
	return num;
}

/*

	sendJson
	
	Given a json formatted argument, and a URL as a string,
	we send the json file to the web, specified by the URL.

*/

bool sendJson(json json, std::string URI) {

	CURL* curl = curl_easy_init();

	if (curl) {
		std::cout << "Success initializing curl to send json...";
		std::cout << std::endl;

		std::string jsonStr = json.dump();

		// liburl loves their const char arrays
		const char *cstr = jsonStr.c_str();
		const char* URIcstr = URI.c_str();

		// We make headers for our HTTP with sending json.

		struct curl_slist *headers = NULL;

		headers = curl_slist_append(headers, "Accept: application/json");
		headers = curl_slist_append(headers, "Content-Type: application/json");
		headers = curl_slist_append(headers, "charsets: utf-8");

		// We set the path to our beloved API.
		curl_easy_setopt(curl, CURLOPT_URL, URIcstr);

		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, cstr);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcrp/0.1");

		CURLcode code = curl_easy_perform(curl);

		curl_easy_cleanup(curl);

		if (code == CURLE_OK) {

			// Note that while we may have succeeded in sending the request as per standard,
			// the json file may not be compatible with the receiving end.
			// However, this project is small, and since the API isn't anyone other than the
			// projectholder, we can safely assume consistency in the format.

			std::cout << std::endl;
			std::cout << "we succeeded in sending the json thing!";
			std::cout << std::endl << std::endl;
			return true;
		}
		else {
			std::cout << "Curl error code attempting json send: " << code << "." << std::endl;
		}
	}
	else {
		std::cout << "Could not initialize curl..." << std::endl;
	}

	return false;
}

/*

We send a usermade inkopslista to mockapi.

*/

bool sendInkopsList(json* js, std::string APIKEY) {
	// send json format and a URI
	if (sendJson(*js, "http://" + APIKEY + ".mockapi.io/system/inkopslista")) {
		return true;
	}
	return false;
}


/*

Gets every inkopslista in mockapi. They contain an id string and an array of strings called products.
Saves in parameter json pointer.

*/


bool getInkopsList(json* inkopsList, std::string APIKEY) {
	std::string content;

	std::string URI = "http://" + APIKEY + ".mockapi.io/system/inkopslista";

	if (getStringDataFromURI(&content, URI)) {

		try
		{
			*inkopsList = toJson(content);
		}
		catch (json::parse_error& e)
		{
			std::cout << "json parsing error" << std::endl;
			std::cout << "message: " << e.what() << '\n'
				<< "exception id: " << e.id << '\n'
				<< "byte position of error: " << e.byte << std::endl;
			std::cout << std::endl;
			return false;
		}

		return true;
	}

	return false;
}


/*

Basically the same as sending a json file, but we refer to a json object to delete by an ID,
which the user was made aware of from "getInkopsList"


*/


bool deleteInkopslist(std::string ID, std::string APIKEY) {
	CURL* curl = curl_easy_init();

	if (curl) {
		std::cout << "Success initializing curl to send json...";
		std::cout << std::endl;

		std::string URI = "http://" + APIKEY + ".mockapi.io/system/inkopslista/" + ID;

		const char* URIcstr = URI.c_str();

		struct curl_slist *headers = NULL;

		headers = curl_slist_append(headers, "Accept: application/json");
		headers = curl_slist_append(headers, "Content-Type: application/json");
		headers = curl_slist_append(headers, "charsets: utf-8");

		curl_easy_setopt(curl, CURLOPT_URL, URIcstr);

		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcrp/0.1");

		CURLcode code = curl_easy_perform(curl);

		curl_easy_cleanup(curl);

		if (code == CURLE_OK) {
			std::cout << std::endl;
			std::cout << "we succeeded in sending the delete request.";
			std::cout << std::endl << std::endl;
			return true;
		}
		else {
			std::cout << "Curl error code attempting DELETE: " << code << "." << std::endl;
		}
	}
	else {
		std::cout << "Could not initialize curl..." << std::endl;
	}

	return false;
}



/*

worldtimeapi API

This is self explanatory.

The reason for saving it in a json and not two strings (date and time)
is because we might want to do other thing

*/

bool getTime(json* timeJson) {

	std::string URI = "http://worldtimeapi.org/api/timezone/Europe/Stockholm";
	std::string content;

	if (getStringDataFromURI(&content, URI)) {
		try {
			*timeJson = toJson(content);
		}
		catch (json::parse_error& e)
		{
			std::cout << "json parsing error" << std::endl;
			std::cout << "message: " << e.what() << '\n'
				<< "exception id: " << e.id << '\n'
				<< "byte position of error: " << e.byte << std::endl;
			std::cout << std::endl;
			// json failure
			return false;
		}
		// success!
		return true;
	}
	// getting failure
	return false;
}

/*

Gets datetime json, splits it into date and time.

*/

void divideTime(json timejson, std::string* date, std::string* time) {

	// Data is in format 2019-05-13T12:12:32.958174+02:00
	std::string t = timejson["datetime"].get<std::string>();
	// We strip it to 2019-05-13 12:12:32 958174+02:00
	std::replace(t.begin(), t.end(), 'T', ' ');
	std::replace(t.begin(), t.end(), '.', ' ');
	// Then we only get:
	//	2019-0-03 >> date
	//	12:12:32 >> time
	// and we don't care about the rest and leave it in the dust.
	std::stringstream ss(t);
	ss >> *date;
	ss >> *time;
}

/*

Sends time to mockapi

*/

bool sendTime(json* time, std::string APIKEY) {

	// send json format and a URI
	if (sendJson(*time, "http://" + APIKEY + ".mockapi.io/system/time")) {
		return true;
	}
	return false;
}


int wmain(int argc, wchar_t *argv[]) {

	curl_global_init(CURL_GLOBAL_ALL);

	json APIKEYS = toJson(getFileContents("APIKEYS.txt"));

	std::vector<Location> locations;				// We can use these to find nearby location names, as well as weather.
	std::vector<std::string> stopLocationNames;		// We can use these to find stations. A crucial step, because they give us something to search for in platsuppslag.
	std::vector<Station> stations;					// We can use these to find departures because they have the necessary components.

	
	json inkopsListToSend;		// User made inkopslist
	json inkopsListsGotten;			// Gotten inkopslists
	std::string productInput;	// just some input
	bool productInputting;		// logic stuff
	bool productExit;			// more logic stuff

	json timejson;			// we store the timejson here
	json SLjson;			// we store departures here
	json locationjson;		// we store locations here temporarily
	json weatherjson;		// we store weather here.

	std::string date;						// We print this later when we've filled it
	std::string time;						// -||-

	bool gotWeather				= false;	// got weather from api
	bool gotInkopsList			= false;	// got inkopslist from pi

	bool gotLocation			= false;	// etc
	bool gotNearbyStopNames		= false;	// etc
	bool gotSiteID				= false;	// etc
	bool gotStations			= false;	// etc
	bool gotDepartures			= false;	// etc

	int timeWindowMinutes = 60;		// that's the max and we don't allow the user to input it.
	int nearbyStationMax = 9;		// 9 stations
	int nearbyStationRadius = 2000;	// 2000 meter radius

	std::string locationQ;	// location question.
	std::string lat;		// latitude
	std::string lon;		// longitude

	bool requestingStuff = true; // Main menu loop boolean
	int input;	// we use this sometimes when we choose stuff.


	// This is the main menu
	// The main menu could be trimmed down.
	// But I worked on this more than the hours we have allotted in the week for self studies and I gave up on my weekends
	// just to finish this program, so that will not happen.
	while (requestingStuff) {

		std::cout << "[-1] quit" << std::endl;
		std::cout << "[0] Write place and get SL traffic! (maybe)" << std::endl;
		std::cout << "[1] Get weather!" << std::endl;
		std::cout << "[2] Get time!" << std::endl;
		std::cout << "[3] Make&post inkopslist" << std::endl;
		std::cout << "[4] Get inkopslist" << std::endl;
		std::cout << "[5] Delete an inkopslist" << std::endl;
		std::cout << "[6] Send time" << std::endl;

		std::cout << "Input: ";
		
		input = getInputInt(-1, 6);
		std::cout << std::endl;

		// Some consistency stuff that should not bleed over each time.
		gotLocation = false;
		gotNearbyStopNames = false;
		gotSiteID = false;
		gotStations = false;
		gotDepartures = false;

		switch (input) {
		/*
		---------------------------------------------------.
			CASE -1: BREAK THE MAIN LOOP.S
		---------------------------------------------------.
		*/
		case -1:
			requestingStuff = false;
			break;

		/*
		---------------------------------------------------.
			CASE 0: GETTING TRAFFIC FROM LOCAITON
						USER: ENTER PLACE
						PROGRAM: GETS LOCATIONS
						USER: CHOOSES BETWEEN LOCATIONS
						PROGRAM: FINDS NEARBY SL STOPS
						USER: CHOOSES SL STOP
						PROGRAM: FINDS ACTUAL STATIONS
						USER: CHOOSES AN ACTUAL STATION
						PROGRAM: GETS DEPARTURES FOR CHOSEN STATION.
		---------------------------------------------------.
		*/
		case 0:
			std::cout << "Enter a place (preferably in stockholms lan) to find traffic: ";
			std::cin >> locationQ;
			locations.clear();

			if (getLocation(&locations, locationQ, APIKEYS["locationiq"].get<std::string>())) {
				if (locations.size() > 1) {
					for (int i = 0; i < locations.size(); i++) {
						std::cout << "[" << i << "] ";
						std::cout << locations[i].name;
						std::cout << std::endl;
					}
					std::cout << std::endl;
					std::cout << "Choose a location: ";

					input = getInputInt(0, locations.size() - 1) % locations.size();
					lat = locations[input].lat;
					lon = locations[input].lon;
				}
				else {
					// There was only one location, therefore we select it by default.
					// This has never happened during testing, but just for safety.
					// Or well not safety, more like feeling like it saves the user some time.
					std::cout << "Only one location: " << locations[0].name << ", continuing..." << std::endl;
					lat = locations[0].lat;
					lon = locations[0].lon;
				}

				std::cout << std::endl;

				// Clear this because of safety. The results from getLocations() is not relevant anymore.
				// Maybe we could use it later for weather stuff, or get weather here too.
				// But no, we clear it.
				locations.clear();

				// Get nearby stop names. All we do here is call SL Närliggande Hållplatser 2 to get names into our stopLocationNames vector
				// We send the location we chose.
				gotNearbyStopNames = getNearbyStopNames(&stopLocationNames, lat, lon, nearbyStationMax, nearbyStationRadius, APIKEYS["nearby"]);

				if (gotNearbyStopNames) {
					int chosenStopLocation = 0;

					// The stop location names we got, around the LOCATION we chose.
					if (stopLocationNames.size() > 1) {
						std::cout << "...Found these stops..." << std::endl;

						for (int i = 0; i < stopLocationNames.size(); i++) {
							std::cout << "[" << i << "]" + stopLocationNames[i] << std::endl;
						}

						// We choose between the stop locations
						std::cout << std::endl;
						std::cout << "Choose a stop: ";

						chosenStopLocation = getInputInt(0, stopLocationNames.size() - 1) % (stopLocationNames.size());
					}
					else {
						std::cout << "Only found one stop location: " << stopLocationNames[0] << ", continuing..." << std::endl;
					}

					// The stop location we chose is what we query SL Platsuppslag API with, because they want a string of a station name
					// and I want a siteID coupled with a station name. That's why we send a vector of Station which have name and station.
					// Stations will be created if successful.
					gotStations = getStationsFromStopName(&stations, stopLocationNames[chosenStopLocation], APIKEYS["platsuppslag"]);

					// We clear stoplocationnames, because all we do with it is push back elements and read them out, and query with one of them.
					// We want to make this not have information information so that we, next time, push back stop location names into an empty vector.
					stopLocationNames.clear();
				}
				else {
					std::cout << "Could not get any stop names. :<" << std::endl;
				}

				// If we got stations then we 
				if (gotStations) {
					for (unsigned int i = 0; i < stations.size(); i++) {
						std::cout << std::endl;
						std::cout << "[" << i << "] ";
						std::cout << "Station " << stations[i].getName();
						std::cout << ", siteID: " << stations[i].getSiteID();
					}

					// Again, we choose the best match station. 
					std::cout << std::endl;
					std::cout << "Which station would you like to choose?";
					std::cout << std::endl << "Input: ";

					int chosenStation = getInputInt(0, stations.size() - 1) % (stations.size());

					// Since we have valid siteID's now, we can query realtimedepartures API
					gotDepartures = getDepartures(&SLjson, stations[chosenStation].getSiteID(), timeWindowMinutes, APIKEYS["realtimedepartures"]);

					stations.clear();
				}
				else {
					std::cout << "Could not get stations :C" << std::endl;
				}

				// Print if we got the departures.
				// Looks stupid but I found it's intuitive.
				if (gotDepartures) {

					// These become json::iterators 
					// With it we access for example 
					// "Metros": [{departurestuff}, {departurestuff}]
					auto BusesPtr = SLjson["ResponseData"].find("Buses");
					auto MetrosPtr = SLjson["ResponseData"].find("Metros");
					auto TrainsPtr = SLjson["ResponseData"].find("Trains");
					auto TramsPtr = SLjson["ResponseData"].find("Trams");
					auto ShipsPtr = SLjson["ResponseData"].find("Ships");

					// we iterate through the elements of (e.g.) Metros in printTransport
					// to print them.
					printTransport(BusesPtr, SLjson);
					printTransport(MetrosPtr, SLjson);
					printTransport(TrainsPtr, SLjson);
					printTransport(TramsPtr, SLjson);
					printTransport(ShipsPtr, SLjson);
				}
				else {
					std::cout << "Could not get any departures. :/" << std::endl;
				}

			}
			else {
				std::cout << "Could not get any locations. X,(" << std::endl;
			}

			break;

		/*
		---------------------------------------------------.
			CASE 1: GETTING WEATHER
						USER: ENTER PLACE
						PROGRAM: GETS LOCATIONS
						USER: CHOOSES BETWEEN LOCATIONS
						PROGRAM: FINDS WEATHER
						PRGORAM: PRINTS WEATHER SIMPLY
		---------------------------------------------------.
		*/
		case 1:
			std::cout << "Enter a location for weather: ";
			std::cin >> locationQ;
			std::cout << std::endl;
			// clear for safety.
			locations.clear();
			// Query locationIQ with inputted location.
			if (getLocation(&locations, locationQ, APIKEYS["locationiq"].get<std::string>())) {
				if (locations.size() > 1) {

					for (int i = 0; i < locations.size(); i++) {
						std::cout << "[" << i << "] ";
						std::cout << locations[i].name;
						std::cout << std::endl;
					}
					// Choose between resulted locations
					std::cout << std::endl;
					std::cout << "Choose a location: ";
					int choice = getInputInt(0, locations.size() - 1) % locations.size();
					lat = locations[choice].lat;
					lon = locations[choice].lon;
				}
				else {
					// unless there is only one, then there is no choice.
					lat = locations[0].lat;
					lon = locations[0].lon;
				}

				std::cout << std::endl;
				// Contacts openweather API with our chosen location
				gotWeather = getWeather(&weatherjson, lat, lon, APIKEYS["openweather"].get<std::string>());

				// Describes the weather
				if (gotWeather) {
					json::iterator weather = weatherjson["weather"].begin();
					std::cout << "Weather: " << weather.value()["main"].get<std::string>();
					std::cout << std::endl;
					std::cout << "More descriptive weather: " << weather.value()["description"].get<std::string>();
					std::cout << std::endl;
					std::cout << "temperature: ";
					std::cout << weatherjson["main"]["temp"].get<float>();
					std::cout << " degrees celsius";
				}
				else {
					std::cout << "Could not get the weather.";
				}
				std::cout << std::endl;
				std::cout << std::endl;

				// for safety again.
				locations.clear();
			}
			else {
				std::cout << std::endl;
				std::cout << "Could not find such a location." << std::endl;;
			}
			break;

		/*
		---------------------------------------------------.
			CASE 2: TIME
						USER: ENTER NOTHING
						PROGRAM: GETS TIME FOR CENTRAL EUROPEAN TIME
		---------------------------------------------------.
		*/
		case 2:

			// Here we get the time from worldtimeapi API.

			if (getTime(&timejson)) {

				divideTime(timejson, &date, &time);
				// Describe the time.
				std::cout << "Date: " << date << std::endl;
				std::cout << "Time: " << time << std::endl;

				// From here we could use some cool time functions to display time always, now 
				// that we know the time. That's pretty cool.
			}
			break;
		/*
		---------------------------------------------------.
			CASE 3: MAKING INKOPSLIST
						USER: ENTERS PRODUCTS 
						PROGRAM: ADDS PRODUCTS TO JSON ARRAY
						USER: SIGNALS DONE
						PROGRAM: SENDS PRODUCTS AS MOCKAPI-COMPATIBLE JSON
		---------------------------------------------------.
		*/
		case 3:
			std::cout << "Making a list." << std::endl;
			std::cout << "Enter your product, or" << std::endl;
			std::cout << "Enter \"exit\" to exit, or" << std::endl;
			std::cout << "Enter \"done\" to done" << std::endl;

			// This is cool. We set the json object "products" to a nlohmann array,
			// as it is set up on mockapi.
			inkopsListToSend["products"] = json::array();
			productInputting = true;
			productExit = false;

			// Input a product, exit (to throw away), or done (to send)
			while (productInputting) {
				std::cout << "Input: ";
				std::cin >> productInput;

				if (productInput == "exit" || productInput == "done") {
					productInputting = false;
					if (productInput == "exit") {
						productExit = true;
					}
				}
				else {
					// If input is anything other than exit or done, the
					// input is obviously a product.
					inkopsListToSend["products"].push_back(productInput);
				}
			}

			std::cout << std::endl;

			// This could be done in a loop, such that the user does not
			// lose their inkopslist when sending it fails, which would be nice.

			if (!productExit) {

				// Send to mockapi, really cool. 
				if (sendInkopsList(&inkopsListToSend, APIKEYS["mockapi"].get<std::string>())) {
					std::cout << "Success sending inkopslista" << std::endl;
					gotInkopsList = false;
				}
				else {
					std::cout << "Failure sending inkopslista!" << std::endl;
				}
			}
			break;
		/*
		---------------------------------------------------.
			CASE 4: GETTING INKOPSLIST
						USER: ENTERS NOTHING
						PROGRAM: GETS EVERY INKOPSLIST FROM MOCKAPI
						PROGRAM: PRINTS EVERY INKOPSLIST
		---------------------------------------------------.
		*/
		case 4:
			// Get inkopslist like usual json. libcurl, json.
			gotInkopsList = getInkopsList(&inkopsListsGotten, APIKEYS["mockapi"].get<std::string>());

			// Then we simply print it. This is just a get and print thing.

			if (gotInkopsList) {
				bool any = false;

				// We iterate through the inkopslist json file, which is in the format of 
				// [{"id":"1", "products":["apple","bread"]}]
				// mockapi is a list of inkopslists, so we iterate through each inkopslist.
				for (json::iterator list = inkopsListsGotten.begin(); list != inkopsListsGotten.end(); ++list) {

					// In each iteration, we print the ID of the inkopslist, 
					std::cout << "ID: " << list.value()["id"].get<std::string>();
					std::cout << std::endl;
					any = true;
					// and we iterate through the product string array.
					for (json::iterator products = list.value()["products"].begin(); products != list.value()["products"].end(); ++products) {
						std::cout << products.value().get<std::string>();
						std::cout << std::endl;
					}
					std::cout << std::endl;
				}
				if (!any) {
					std::cout << "There are no inkopslists!!!" << std::endl;
				}
			}
			break;
		/*
		---------------------------------------------------.
			CASE 5: DELETE GOTTEN INKOPSLIST
						USER: ENTERS NOTHING
						PROGRAM: GETS AND PRINTS EVERY INKOPSLIST FROM MOCKAPI
						USER: CHOOSES A ID OF AN INKOPSLIST
						PROGRAM: DELETES THE DESIRED INKOPSLIST WITH DELETE REQUEST
		---------------------------------------------------.
		*/
		case 5:
			// We get inkopslists so that the user can know which are removable.
			// Which in this implementation is every inkopslist.
			// Print them, expose the ID per list, and user chooses to remove one.
			if (getInkopsList(&inkopsListsGotten, APIKEYS["mockapi"].get<std::string>())) {
				int i = 0;
				for (json::iterator list = inkopsListsGotten.begin(); list != inkopsListsGotten.end(); ++list) {
					std::cout << "ID: " << list.value()["id"].get<std::string>();
					std::cout << " ";
					i++;
					// There is no prettyprint here, just get them out of there so we knwo what the inkopslist is about.
					for (json::iterator products = list.value()["products"].begin(); products != list.value()["products"].end(); ++products) {
						std::cout << products.value().get<std::string>();
					}
					std::cout << std::endl;
				}

				std::cout << std::endl;
				std::cout << "Enter an inkopslist ID to remove: ";
				input = getInputInt(0, MAXINT);

				std::cout << std::endl;

				if (deleteInkopslist(std::to_string(input), APIKEYS["mockapi"].get<std::string>())) {
					std::cout << "Deletion successful! :)" << std::endl;
					// False in case we want to delete another one. The old one is still present
					// in inkopsListGot, which means that we will need to get a new one.
					gotInkopsList = false;
				}
				else {
					std::cout << "Deletion unsuccessful... :(" << std::endl;
				}
			}
			else
			{
				std::cout << "Could not get inkopslists." << std::endl;
			}
			break;
		/*
		---------------------------------------------------.
			CASE 6: GET TIME AND POST IT TO MOCKAPI
						USER: ENTERS NOTHING
						PROGRAM: GETS TIME
						PROGRAM: SENDS TIME AS MOCKAPI-COMPATIBLE JSON
		---------------------------------------------------.
		*/
		case 6:
			// we get the time
			if (getTime(&timejson)) {
				// We divide it into date and time
				divideTime(timejson, &date, &time);
				json timejs;
				timejs["date"] = date;
				timejs["time"] = time;
				// send json representation of it.
				sendTime(&timejs, APIKEYS["mockapi"].get<std::string>());
			}
			break;
		}

		std::cout << std::endl;
	}

	// Cleanup before exiting. Important to delete in-library pointers and memory allocation handling.
	curl_global_cleanup();

	std::cin.get();
	return 0;
}
