# curljson
Using libcurl and nlohmann's library for json to practice API, HTTP with system integration as umbrella.


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
