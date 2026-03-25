Projekt: Visual Studio 2022 + DirectX 12 + 3 bryly 3D z teksturami DDS

Wazna uwaga:
DDS jest formatem tekstur, nie modeli 3D. W tym projekcie bryly 3D sa tworzone proceduralnie w kodzie (szescian, piramida, osmioscian), a pliki DDS sa wczytywane jako tekstury i nakladane na te bryly.

Co zawiera paczka:
- plik rozwiazania .sln
- projekt .vcxproj zgodny z Visual Studio 2022
- src/main.cpp
- Shader.hlsl
- folder assets z 3 teksturami DDS

Wymagania:
- Visual Studio 2022
- workload: Desktop development with C++
- Windows 10/11 SDK z obsluga DirectX 12
- kompilacja dla platformy x64

Uruchomienie:
1. Rozpakuj ZIP.
2. Otworz plik VS2022_DirectX12_3BrylyDDS.sln w Visual Studio 2022.
3. Wybierz konfiguracje Debug lub Release oraz platforme x64.
4. Zbuduj projekt (Build -> Build Solution).
5. Uruchom (F5).

Efekt:
- otworzy sie okno DirectX 12,
- pojawia sie trzy bryly 3D,
- kazda bryla obraca sie wzgledem wlasnej osi,
- kazda bryla ma inna teksture DDS.

Uwagi techniczne:
- projekt nie wymaga NuGet ani DirectXTK12,
- zastosowano prosty loader DDS dla tekstur RGBA8,
- shadery sa kompilowane w czasie uruchomienia z pliku Shader.hlsl.
