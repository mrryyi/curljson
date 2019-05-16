# curljson
Using libcurl and nlohmann's library for json to practice API, HTTP with system integration as umbrella.

To run the program's inkopslist and time sending functionality, you will need to set up mockapi somewhat.
I won't tell you how to do that, because the functionality is so dumb. The program just sends up a string array, can get it, can delete lists. That's all you need to know really.

You will need a text file called "APIKEYS.txt" formatted as such:

{"locationiq":"<yourkey>",
"openweather":"<yourkey>",
"realtimedepartures":"<yourkey>",
"platsuppslag":"<yourkey>",
"nearby":"<yourkey>",
"mockapi":"<yourkey>"}
  
locationiq: https://locationiq.com/docs#forward-geocoding
openweather: https://openweathermap.org/api
realtimedepartures: https://www.trafiklab.se/api/sl-realtidsinformation-4
platsuppslag: https://www.trafiklab.se/api/sl-platsuppslag
nearby: https://www.trafiklab.se/api/sl-narliggande-hallplatser-2
mockapi: https://www.mockapi.io/

You can still run the program, but it's not going to be able to get information from the API's you don't have a key for.

If you want my API keys, send me an email at: jrhagelin@gmail.com. Abuse will not be tolerated, as there are only so many requests my API keys are permitted.


Developing the main.cpp further requires knowledge of how to install libcurl for your system and IDE. Personally I followed this video: https://www.youtube.com/watch?v=wjNyT5PhNvI

With the additions of modifying my microsoft visual studio community installation to have "Visual C++ MFC for x86 and x64" checked under "Individual components" in the Visual Studio Installer.

I also included these following libraries in the Linker/Input/Additional dependencies.

libcurl.lib
Normaliz.lib
Ws2_32.lib
Wldap32.lib
Crypt32.lib
advapi32.lib

To develop with nlohmann's json library, as is used extensively in the project, include a filepath to the include library in your project's C/C++/General/Additional libraries. For example:
  C:\json-develop\include
