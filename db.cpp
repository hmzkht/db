#include <iostream>
#include <stdio.h>
#include <string>
#include <string.h>
#include <algorithm>
#include <vector>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <fstream>
#include "BTree.h"

#define MIN_DEG 2

using namespace std;

int define_table(string table_name, int numcol, vector<string> column_names, int *which);
int insert_row(string table_name, int numvals, vector<string> values);
int fetch_rows(string table_name, string projection, string condition);
int distinct_fetch_rows(string table_name, string projection, string condition);
int parse_command(string command);
int drop_table(string table_name);
void refresh();

vector<string> dirList;
struct tbl {
	string name;
	vector<string> cols;
	int numrows, numblocks;
};
vector<tbl> tbl_lookup;
bool debug;
vector<BTree> indexes;

int main(int argc, char *argv[])
{
    string cmd, line, tmp, col;
	stringstream ss;
	int quit;
	if(argc==2) {
		if(!strcmp(argv[1],"-d"))
			debug = true;
	}
	
	//populate dir list
	DIR *d;
	struct dirent *dir;
	ifstream file;
	int i = 0;
	d = opendir("./data");
	if(d) {
		while((dir = readdir(d)) != NULL) {
			i++;
			dirList.push_back(dir->d_name);
		}
	}
	closedir(d);
	
	//populate lookup table and indexes
	for(int j = 0; j < dirList.size(); j++) {
		tmp = dirList[j].substr(0,dirList[j].size()-4);
		if((tmp == ".") || (tmp == "..")) {
			continue;
		}
		else {
			//don't read .idx files
			int found = tmp.find(".");
			int nrow = 0;
			if(found == string::npos) {
				tbl_lookup.push_back(tbl());
				tbl_lookup[tbl_lookup.size()-1].name = tmp;
				file.open(("./data/"+dirList[j]).c_str());
				getline(file,line);
				ss << line;
				while(ss >> col) {
					tbl_lookup[tbl_lookup.size()-1].cols.push_back(col);
				}
				while(getline(file,line)) {
					nrow++;
				}
				tbl_lookup[tbl_lookup.size()-1].numrows = nrow;
				struct stat st;
				stat(("./data/"+dirList[j]).c_str(), &st);
				tbl_lookup[tbl_lookup.size()-1].numblocks = st.st_blocks;
				file.close();
				ss.clear();
			}
			else {
				char tbl[256],col[256];
				sscanf(tmp.c_str(), "%[_a-zA-Z0-9].%[_a-zA-Z0-9]", tbl,col);
				string table(tbl);
				string column(col);
				BTree idx(MIN_DEG, table,column);
				indexes.push_back(idx);
				ifstream indexFile(("./data/"+table+"."+column+".idx").c_str());
				string val;
				int off;
				while(indexFile >> val >> off)
				indexes[indexes.size()-1].insert(val, off);
				if(debug) cout << "DEBUG: " << tmp << " has a BTree index." << endl;
			}
		}
	}
	
	//main loop
    while(getline(cin,cmd,';')) {
		//refresh the directory list
		DIR *d;
		struct dirent *dir;
		ifstream file;
		int i = 0;
		d = opendir("./data");
		if(d) {
			while((dir = readdir(d)) != NULL) {
				for(int j = 0; j < dirList.size(); j++) {
					if(dirList[j] == dir->d_name)
						break;
					else{
						if(j == dirList.size()-1) {
							i++;
							dirList.push_back(dir->d_name);
						}
					}
				}
			}
		}
		closedir(d);
		
		//do stuff with the input
        cmd.append(";");
		quit = parse_command(cmd);
		getchar();
		if(quit == -1)
			return 0;
    }
}


int parse_command(string command) {
	int n = 0, ncols = 0, error;
	char table_name[256], values[256], projection[256], condition[256];
	vector<string> cols;
	for(int i = 0; i < command.length(); i++) {
		if(command[i] >= 97 && command[i] <= 122)
			command[i] -= 32;
	}
	//insert row
	n = sscanf(command.c_str()," INSERT INTO %[_a-zA-Z0-9] VALUES %[^;]", table_name, values);
	if(n) {
		//change from c strings to c++ strings so we can use string methods
	    string tbl = string(table_name);
	    string vals = string(values);
		//ignore all whitespace
	    vals.erase(remove_if(vals.begin(),vals.end(),::isspace),vals.end());
		//remove parentheses for easier tokenizing
		vals.erase(remove(vals.begin(),vals.end(),'('),vals.end());
		vals.erase(remove(vals.begin(),vals.end(),')'),vals.end());
		
		//tokenize
		istringstream iss(vals);
		string token;
		while (getline(iss, token, ','))
		{
			cols.push_back(token);
		}
		ncols = cols.size();
		error = insert_row(tbl, ncols, cols);
		int neededcols;
		for(int i = 0; i < tbl_lookup.size(); i++) {
			if(tbl_lookup[i].name == table_name) {
				neededcols = tbl_lookup[i].cols.size();
				break;
			}
		}
		switch(error) {
			case 0:
				cout << "Success." << endl;
				return error;
			case 1:
				cout << "ERROR: Table does not exist." << endl;
				return error;
			case 2:
				cout << "ERROR: Too many values. Recieved: " << cols.size() << " Expected: " << neededcols << endl;
				return error;
			case 3:
				cout << "ERROR: Too few values. Recieved: " << cols.size() << " Expected: " << neededcols << endl;
				return error;
			case 4: 
				cout << "ERROR: Unknown server error." << endl;
				return error;
		}
	}
	
	//create table
	n = sscanf(command.c_str()," CREATE TABLE %[_a-zA-Z0-9] (%[^;]", table_name, values);
	if(n) {
		//change from c strings to c++ strings so we can use string methods
	    string tbl = string(table_name);
	    string vals = string(values);
		//remove parentheses for easier tokenizing
		vals.erase(remove(vals.begin(),vals.end(),'('),vals.end());
		vals.erase(remove(vals.begin(),vals.end(),')'),vals.end());
		//tokenize
		istringstream iss(vals);
		string token;
		while (getline(iss, token, ','))
		{
			cols.push_back(token);
		}
		
		ncols = cols.size();
		//indicate which columns are primary or key
		int whichkeys[ncols];
		for(int i = 0; i < ncols; i++) {
			whichkeys[i] = 0;
		}
		vector<string> temp;
		for(int i = 0; i < cols.size(); i++) {
			istringstream tmp(cols[i]);
			temp.clear();
			while(tmp >> token) {
				temp.push_back(token);
			}
			if(temp.size() == 2 && (temp[1] == "PRIMARY" || temp[1] == "KEY")) {
				cols[i] = temp[0];
				if(temp[1] == "PRIMARY") {
					whichkeys[i] = 1;
				}
				else if(temp[1] == "KEY") {
					whichkeys[i] = 2;
				}
				else {
					whichkeys[i] = 0;
				}
				cout << cols[i] << " is " << temp[1] << endl;
			}
			cols[i].erase(remove_if(cols[i].begin(),cols[i].end(),::isspace),cols[i].end());
		}
		
		error = define_table(tbl, ncols, cols, whichkeys);
		switch(error){
			case 0:
				cout << "Success." << endl;
				return error;
			case 1: 
				cout << "ERROR: Table already exists." << endl;
				return error;
			case 2:
				cout << "ERROR: Cannot create database directory. Please check your user permissions." << endl;
				return error;
			case 3:
				cout << "ERROR: Invalid table name. Names must contain only alphanumeric and underscore characters." << endl;
				return error;
			case 4:
				cout << "ERROR: Invalid column name. Names must contain only alphanumeric and underscore characters." << endl;
				return error;
			case 5: 
				cout << "ERROR: Unknown server error." << endl;
				return error;
		}
	}
	
		
	//get rows with distinct projection
	n = sscanf(command.c_str()," SELECT DISTINCT %[,_a-zA-Z0-9] FROM %[_a-zA-Z0-9] WHERE %[^;] ;", projection, table_name, condition);
	if(n) {
		string proj = string(projection);
		string tbl = string(table_name);
		string cond = string(condition);
		error = distinct_fetch_rows(tbl, proj, cond);
		switch(error){
			case 0:
				cout << "Success." << endl;
				return error;
			case 1: 
				cout << "ERROR: Table doesn't exist." << endl;
				return error;
			case 2:
				cout << "ERROR: Projection column doesn't exist in the specified table." << endl;
				return error;
			case 3:
				cout << "ERROR: Selection column doesn't exist in the specified table." << endl;
				return error;
			case 4: 
				cout << "ERROR: Unknown server error." << endl;
				return error;
		}
	}
	
	//get rows
	n = sscanf(command.c_str()," SELECT %[,_a-zA-Z0-9] FROM %[_a-zA-Z0-9] WHERE %[^;] ;", projection, table_name, condition);
	if(n) {
		string proj = string(projection);
		string tbl = string(table_name);
		string cond = string(condition);
		error = fetch_rows(tbl, proj, cond);
		switch(error){
			case 0:
				cout << "Success." << endl;
				return error;
			case 1: 
				cout << "ERROR: Table doesn't exist." << endl;
				return error;
			case 2:
				cout << "ERROR: Projection column doesn't exist in the specified table." << endl;
				return error;
			case 3:
				cout << "ERROR: Selection column doesn't exist in the specified table." << endl;
				return error;
			case 4: 
				cout << "ERROR: Unknown server error." << endl;
				return error;
		}
	}

	
	//drop table
	n = sscanf(command.c_str()," DROP TABLE %[_a-zA-Z0-9] ;", table_name);
	if(n) {
		string tbl = string(table_name);
		error = drop_table(tbl);
		switch(error) {
			case 0:
				cout << "Success." << endl;
				return error;
			case 1:
				cout << "ERROR: Table doesn't exist." << endl;
				return error;
		}
	}
	
	//quit
	command.erase(remove_if(command.begin(),command.end(),::isspace),command.end());
	if(command == "QUIT;")
		return -1;
	
	if(command == "INFO;") {
		cout << "TABLE STATS" << endl;
		cout << "TABLE\tNUMROWS\tNUMBLOCKS" << endl;
		for(int i = 0; i < tbl_lookup.size(); i++) {
			cout << tbl_lookup[i].name << '\t' << tbl_lookup[i].numrows << '\t' << tbl_lookup[i].numblocks << endl;
		}
		cout << "INDEX STATS" << endl;
		cout << "TABLE\tCOLUMN\tDISTINCT VALUES" << endl;
		for(int i = 0; i < indexes.size(); i++) {
			cout << indexes[i].table << '\t' << indexes[i].column << '\t' << indexes[i].distinctvals << endl;
		}
		cout << "Success." << endl;
		return 0;
	}
	
	if(command == "REFRESH;") {
		refresh();
		cout << "Success." << endl;
		return 0;
	}
	
	//no pattern match
	cout << "Invalid command." << endl;
	return 0;
}

//0 success, 1 tbl doesnt exist
int drop_table(string table_name) {
	//check if tbl exists
	for(int i = 0; i < dirList.size(); i++) {
		if(dirList[i] == (table_name+".tbl")) {
			break;
		}
		else {
			if(i == dirList.size()-1) {
				return 1;
			}
		}
	}
	
	remove(("./data/"+table_name+".tbl").c_str());
	for(int i = 0; i < indexes.size(); i++) {
		if(indexes[i].table == table_name)
			remove(("./data/"+indexes[i].table+"."+indexes[i].column+".idx").c_str());
	}
	for(int i= 0; i < dirList.size();i++) {
		if(dirList[i] == (table_name+".tbl")) {
			swap(dirList[i],dirList[dirList.size()-1]);
			dirList.pop_back();
		}
	}
	for(int i = 0; i < indexes.size(); i++) {
		if(indexes[i].table == table_name) {
			swap(indexes[i], indexes[indexes.size()-1]);
			indexes.pop_back();
		}
	}
	for(int i = 0; i < tbl_lookup.size(); i++) {
		if(tbl_lookup[i].name == table_name) {
			swap(tbl_lookup[i], tbl_lookup[tbl_lookup.size()-1]);
			tbl_lookup.pop_back();
		}
	}
	return 0;
}

//0 success, 1 tbl doesnt exist, 2 too many values, 3 too few values, 4 unknown server error
int insert_row(string table_name, int numvals, vector<string> values) {	
	//check if tbl exists
	for(int i = 0; i < dirList.size(); i++) {
		if(dirList[i] == (table_name+".tbl")) {
			break;
		}
		else {
			if(i == dirList.size()-1) {
				return 1;
			}
		}
	}
	
	//check if column names valid
	for(int i = 0; i < values.size(); i++) {
		string tmp = values[i];
		for(int j = 0; j < values[i].length(); j++) {
			if(!((tmp[j] >= 'A' && tmp[j] <= 'Z') || (tmp[j] >= 'a' && tmp[j] <= 'z') 
			|| (tmp[j] >= '0' && tmp[j] <= '9') || (tmp[j] == '_'))) {
				return 4;
			}
		}
	}

	string columns;
	ifstream inFile(("./data/"+table_name+".tbl").c_str());
	if(inFile.is_open()) {
		getline(inFile, columns);
		int numcols = count(columns.begin(),columns.end(),'\t');
		if(numvals > numcols) {
			return 2;
		}
		else if(numvals < numcols){
			return 3;
		}
		else {
			ofstream outFile(("./data/"+table_name+".tbl").c_str(), ofstream::out | ofstream::app);
			int pos = outFile.tellp();
			//cout << pos << endl;
			for(int i = 0; i < numvals; i++) {
				outFile << values[i] << "\t";
			}
			outFile << endl;
			
			//update indexes
			for(int i = 0; i < indexes.size(); i++) {
				//table name match
				if(indexes[i].table == table_name) {
					//find the column
					for(int j = 0; j < tbl_lookup.size(); j++) {
						if(tbl_lookup[j].name == table_name) {
							for(int k = 0; k < tbl_lookup[j].cols.size(); k++) {
								if(indexes[i].column == tbl_lookup[j].cols[k]) {
									//insert the value and offset
									indexes[i].insert(values[k].c_str(),pos);
									ofstream indexFile(("./data/"+table_name+"."+indexes[i].column+".idx").c_str(), ofstream::out | ofstream::app);
									indexFile << values[k] << "\t" << pos << "\t" << endl;
									break;
								}
							}
							break;
						}
					}
				}
			}
			return 0;
		}
	}
}

//0 success, 1 table exists, 2 cant make dir, 3 invalid table name, 4 invalid col name, 5 else
int define_table(string table_name, int numcol, vector<string> column_names, int *which) {
	//create directory if not exist. exit if fail
	struct stat st = {0};
	int test = 0;
	if (stat("./data", &st) == -1) {
		test = mkdir("./data", S_IRWXU | S_IRWXG | S_IRWXO);
		if(test == -1)
			return 2;
	}
	
	//check if table name valid
	for(int i = 0; i < table_name.length(); i++) {
		if(!((table_name[i] >= 'A' && table_name[i] <= 'Z') || (table_name[i] >= 'a' && table_name[i] <= 'z') 
		|| (table_name[i] >= '0' && table_name[i] <= '9') || (table_name[i] == '_'))) {
			return 3;
		}
	}
	
	//check if tbl exists
	for(int i = 0; i < dirList.size(); i ++) {
		if(dirList[i] == (table_name+".tbl")) 
			return 1;
	}
	
	//check if column names valid
	for(int i = 0; i < column_names.size(); i++) {
		string tmp = column_names[i];
		for(int j = 0; j < column_names[i].length(); j++) {
			if(!((tmp[j] >= 'A' && tmp[j] <= 'Z') || (tmp[j] >= 'a' && tmp[j] <= 'z') 
			|| (tmp[j] >= '0' && tmp[j] <= '9') || (tmp[j] == '_'))) {
				return 4;
			}
		}
	}
	
	//create the file and write 1st line
	ofstream outFile(("./data/"+table_name+".tbl").c_str());
	if(outFile.is_open()) {
		tbl_lookup.push_back(tbl());
		tbl_lookup[tbl_lookup.size()-1].name = table_name;
		for(int i = 0; i < column_names.size(); i++) {
			outFile << column_names[i] << "\t";
			tbl_lookup[tbl_lookup.size()-1].cols.push_back(column_names[i]);
		}
		outFile << endl;
		outFile.close();
		//make the index file and structure
		for(int i = 0; i < numcol; i++) {
			if(which[i] == 1 || which[i] == 2) {
				ofstream indexFile(("./data/"+table_name+"."+column_names[i]+".idx").c_str());
				BTree idx(MIN_DEG,table_name,column_names[i]);
				indexes.push_back(idx);
			}
		}
		return 0;
	}
	else {
		return 3;
	}
	
}

//0 success, 1 table doesnt exist, 2 projection column does not exist, 3 select col doesnt exist, 4 else
int fetch_rows(string table_name, string projection, string condition) {
	condition.erase(remove(condition.begin(),condition.end(),';'),condition.end());
	char lhs[256], rhs[256];
	sscanf(condition.c_str(), "%[_a-zA-Z0-9] = %['_a-zA-Z0-9]", lhs, rhs);
	string left, right, line, tok;
	vector<int> proj_idx;
	int sel_idx;
	vector<string> projcols;
	left = lhs;
	right = rhs;
	//tokenize
	istringstream iss(projection);
	string token;
	while (getline(iss, token, ','))
	{
		projcols.push_back(token);
	}
	
	//check if tbl exists
	for(int i = 0; i < dirList.size(); i++) {
		if(dirList[i] == (table_name+".tbl")) {
			break;
		}
		else {
			if(i == dirList.size()-1) {
				return 1;
			}
		}
	}
	
	//check if projection column okay
	for(int i = 0; i < tbl_lookup.size(); i++) {
		//if table found
		if(tbl_lookup[i].name == table_name) {
			//check cols
			for(int k = 0; k < projcols.size(); k++) {
				for(int j = 0; j < tbl_lookup[i].cols.size(); j++) {
					if(tbl_lookup[i].cols[j] == projcols[k]) {
						proj_idx.push_back(j);
						break;
					}
					if(k == projcols.size()-1 && j == tbl_lookup[i].cols.size()-1)
						return 2;
				}
			}
			break;
		}
	}
	
	//check if selection column okay
	for(int i = 0; i < tbl_lookup.size(); i++) {
		//if table found
		if(tbl_lookup[i].name == table_name) {
			//check cols
			for(int j = 0; j < tbl_lookup[i].cols.size(); j++) {
				if(tbl_lookup[i].cols[j] == left) {
					sel_idx = j;
					break;
				}
				if(j == tbl_lookup[i].cols.size()-1)
					return 3;
			}
			break;
		}
	}
	
	ifstream inFile(("./data/"+table_name+".tbl").c_str());
	//use index?
	bool useIndex = false;
	vector<string> cols;
	int i;
	for(i = 0; i < indexes.size(); i++) {
		if(indexes[i].column == left && indexes[i].table == table_name) {
			useIndex = true;
			break;
		}
	}
	if(inFile.is_open()) {
		if(useIndex) {
			if(debug) cout << "DEBUG: Using index selection on " << indexes[i].table << "." << indexes[i].column << endl;
			for(int k = 0; k < proj_idx.size(); k++) {
				cout << projcols[k] << "\t";
			}
			cout << endl;
			BTreeNode *tmp = indexes[i].search(right.c_str());
			int j = 0;
			while(right.c_str() > tmp->keys[j])
				j++;
			vector<int> offs = tmp->offsets[j];
			for(int k = 0; k < offs.size(); k++) {
				inFile.seekg(offs[k]);
				getline(inFile, line);
				istringstream iss(line);
				vector<string> vals;
				while(iss >> tok) {
					vals.push_back(tok);
				}
				if(!vals[sel_idx].compare(right)) {
					for(int i = 0; i < proj_idx.size(); i++) {
						cout << vals[proj_idx[i]] << "\t";
					}
					cout << endl;
				}
			}
		}
		else {
			//1st line is header
			for(int i = 0; i < proj_idx.size(); i++) {
				cout << projcols[i] << "\t";
			}
			cout << endl;
			getline(inFile, line);
			while(getline(inFile, line)) {
				line.erase(remove(line.begin(),line.end(),'\r'),line.end());
				istringstream iss(line);
				vector<string> vals;
				while(getline(iss, tok, '\t')) {
					vals.push_back(tok);
				}
				if(!vals[sel_idx].compare(right)) {
					for(int i = 0; i < proj_idx.size(); i++) {
						cout << vals[proj_idx[i]] << "\t";
					}
					cout << endl;
				}
				iss.clear();
			}
		}
	}
	
	return 0;
}

//0 success, 1 table doesnt exist, 2 projection column does not exist, 3 select col doesnt exist, 4 else
int distinct_fetch_rows(string table_name, string projection, string condition) {
	condition.erase(remove(condition.begin(),condition.end(),';'),condition.end());
	char lhs[256], rhs[256];
	sscanf(condition.c_str(), "%[_a-zA-Z0-9] = %['_a-zA-Z0-9]", lhs, rhs);
	string left, right, line, tok;
	vector<int> proj_idx;
	int sel_idx;
	bool found;
	vector<string> projcols;
	vector<string> distinctcols;
	left = lhs;
	right = rhs;
	//tokenize
	istringstream iss(projection);
	string token;
	while (getline(iss, token, ','))
	{
		projcols.push_back(token);
	}
	
	//check if tbl exists
	for(int i = 0; i < dirList.size(); i++) {
		if(dirList[i] == (table_name+".tbl")) {
			break;
		}
		else {
			if(i == dirList.size()-1) {
				return 1;
			}
		}
	}
	
	//check if projection column okay
	for(int i = 0; i < tbl_lookup.size(); i++) {
		//if table found
		if(tbl_lookup[i].name == table_name) {
			//check cols
			for(int k = 0; k < projcols.size(); k++) {
				for(int j = 0; j < tbl_lookup[i].cols.size(); j++) {
					if(tbl_lookup[i].cols[j] == projcols[k]) {
						proj_idx.push_back(j);
						break;
					}
					if(k == projcols.size()-1 && j == tbl_lookup[i].cols.size()-1)
						return 2;
				}
			}
			break;
		}
	}
	
	//check if selection column okay
	for(int i = 0; i < tbl_lookup.size(); i++) {
		//if table found
		if(tbl_lookup[i].name == table_name) {
			//check cols
			for(int j = 0; j < tbl_lookup[i].cols.size(); j++) {
				if(tbl_lookup[i].cols[j] == left) {
					sel_idx = j;
					break;
				}
				if(j == tbl_lookup[i].cols.size()-1)
					return 3;
			}
			break;
		}
	}
	
	ifstream inFile(("./data/"+table_name+".tbl").c_str());
	//use index?
	bool useIndex = false;
	vector<string> cols;
	int i;
	for(i = 0; i < indexes.size(); i++) {
		if(indexes[i].column == left && indexes[i].table == table_name) {
			useIndex = true;
			break;
		}
	}
	if(inFile.is_open()) {
		if(useIndex) {
			if(debug) cout << "DEBUG: Using index selection on " << indexes[i].table << "." << indexes[i].column << endl;
			for(int k = 0; k < proj_idx.size(); k++) {
				cout << projcols[k] << "\t";
			}
			cout << endl;
			BTreeNode *tmp = indexes[i].search(right.c_str());
			int j = 0;
			while(right.c_str() > tmp->keys[j])
				j++;
			vector<int> offs = tmp->offsets[j];
			for(int k = 0; k < offs.size(); k++) {
				inFile.seekg(offs[k]);
				getline(inFile, line);
				istringstream iss(line);
				vector<string> vals;
				while(iss >> tok) {
					vals.push_back(tok);
				}
				if(!vals[sel_idx].compare(right)) {
					for(int i = 0; i < proj_idx.size(); i++) {
						for(int x = 0; x < distinctcols.size(); x++) {
							if(distinctcols[x] == vals[proj_idx[i]])
								found = true;
						}
						if(!found) {
							cout << vals[proj_idx[i]] << "\t";
							distinctcols.push_back(vals[proj_idx[i]]);
						}
						found = false;
					}
					cout << endl;
				}
			}
		}
		else {
			//1st line is header
			for(int i = 0; i < proj_idx.size(); i++) {
				cout << projcols[i] << "\t";
			}
			cout << endl;
			getline(inFile, line);
			while(getline(inFile, line)) {
				line.erase(remove(line.begin(),line.end(),'\r'),line.end());
				istringstream iss(line);
				vector<string> vals;
				while(getline(iss, tok, '\t')) {
					vals.push_back(tok);
				}
				if(!vals[sel_idx].compare(right)) {
					for(int i = 0; i < proj_idx.size(); i++) {
						for(int x = 0; x < distinctcols.size(); x++) {
							if(distinctcols[x] == vals[proj_idx[i]])
								found = true;
						}
						if(!found) {
							cout << vals[proj_idx[i]] << "\t";
							distinctcols.push_back(vals[proj_idx[i]]);
						}
						found = false;
					}
					cout << endl;
				}
				iss.clear();
			}
		}
	}
	
	return 0;
}

void refresh() {
	string tmp, line, col;
	stringstream ss;
	ifstream file;
	tbl_lookup.clear();
	indexes.clear();
	for(int j = 0; j < dirList.size(); j++) {
	tmp = dirList[j].substr(0,dirList[j].size()-4);
	if((tmp == ".") || (tmp == "..")) {
		continue;
	}
	else {
		//don't read .idx files
		int found = tmp.find(".");
		int nrow = 0;
		if(found == string::npos) {
			tbl_lookup.push_back(tbl());
			tbl_lookup[tbl_lookup.size()-1].name = tmp;
			file.open(("./data/"+dirList[j]).c_str());
			getline(file,line);
			ss << line;
			while(ss >> col) {
				tbl_lookup[tbl_lookup.size()-1].cols.push_back(col);
			}
			while(getline(file,line)) {
				nrow++;
			}
			tbl_lookup[tbl_lookup.size()-1].numrows = nrow;
			struct stat st;
			stat(("./data/"+dirList[j]).c_str(), &st);
			tbl_lookup[tbl_lookup.size()-1].numblocks = st.st_blocks;
			file.close();
			ss.clear();
		}
		else {
			char tbl[256],col[256];
			sscanf(tmp.c_str(), "%[_a-zA-Z0-9].%[_a-zA-Z0-9]", tbl,col);
			string table(tbl);
			string column(col);
			BTree idx(MIN_DEG, table,column);
			indexes.push_back(idx);
			ifstream indexFile(("./data/"+table+"."+column+".idx").c_str());
			string val;
			int off;
			while(indexFile >> val >> off)
			indexes[indexes.size()-1].insert(val, off);
			if(debug) cout << "DEBUG: " << tmp << " has a BTree index." << endl;
		}
	}
}
}