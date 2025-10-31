#include<iostream>
#include<Windows.h>
using namespace std;
int main() {
	RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, nullptr);
	RegSetKeyValueA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", "Neko", REG_SZ, "\"C:\\Program Files\\xymh\\Neko\\Neko.exe\"", sizeof("\"C:\\Program Files\\xymh\\Neko\\Neko.exe\""));
	RegCloseKey(HKEY_CURRENT_USER);
	return 0;
}