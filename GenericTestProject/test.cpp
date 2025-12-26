
#include <string>
#include <iostream>
#include <vector>
#include <utility>
#include <any>
#include <fstream>
using PlgManifest = std::vector <std::pair<std::string, std::any >> ;
PlgManifest ReverseStringManifest(std::string tmpstrL) {
	using namespace std;
	string tmpstrF;
	PlgManifest res;
	for (;;) {
		int end = tmpstrL.find_first_of(';');
		bool exitFlag = true;
		tmpstrF = tmpstrL.substr(0, end);
		if (end + 2 < tmpstrL.size() && end != -1) {
			tmpstrL = tmpstrL.substr(end + 1, tmpstrL.size());
			exitFlag = false;
		}
		end = tmpstrF.find_first_of(':');
		std::pair<std::string, std::any > strp;
		strp.first = std::move(tmpstrF.substr(0, end));
		string valCon = tmpstrF.substr(end + 1, tmpstrF.size());
		if (valCon.at(0) == '\"' && valCon.at(valCon.size() - 1) == '\"') {
			strp.second = valCon.substr(1, valCon.size() - 2);
			valCon.clear();
		}
		else if (isdigit(valCon.at(0))) {
			try{
				strp.second = stoll(valCon);
			}
			catch (const std::invalid_argument& e) {
				cerr << e.what();
				res.clear();
				return std::move(res);
			}
			catch (const std::out_of_range& e) {
				cerr << e.what();
				res.clear();
				return std::move(res);
			}
			valCon.clear();
		}
		else {
			if (valCon == "true") {
				strp.second = true;
			}
			else if (valCon == "false") {
				strp.second = false;
			}
			else {
				res.clear();
				return std::move(res);
			}
		}
		res.push_back(std::move(strp));
		tmpstrF.clear();
		if (exitFlag || tmpstrL.empty()) {
			break;
		}
	}
	return std::move(res);
}

int main() {
	std::string Ct;
	//std::cin >> Ct;
	std::ifstream fin("in.txt");
	fin >> Ct;
	PlgManifest res = ReverseStringManifest(Ct);
	using std::cout;
	using std::endl;
	cout << "Size:" << res.size() << endl;
	for (auto& item : res) {
		cout << item.first << " = ";
		auto &type = item.second.type();
		if (type == typeid(long long)) {
			cout << std::any_cast<long long>(item.second);
		}
		else if (type == typeid(bool)) {
			cout << std::any_cast<bool>(item.second);
		}
		else if (type == typeid(std::string)) {
			cout << std::any_cast<std::string>(item.second);
		}
		cout << endl;
	}
	cout << "Size:" << res.size() << endl;
	__debugbreak();
}