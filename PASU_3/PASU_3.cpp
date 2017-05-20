// PASU_3.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <ctime>

using namespace std;

enum Gender { MALE, FEMALE };
enum GenderPolicy { SAME_GENDER, MALE_ONLY, FEMALE_ONLY, TOGETHER };
enum Request { NEEDED, PREFERRED, DONT_CARE };
enum DoctoringLevel { COMPLETE, PARTIAL, NONE };
enum Tag { UNREGISTERED, REGISTERED, ADMITTED, DISCHARGED };
enum MoveType { CHANGE = 1, SWAP, DELAY, PARTIAL_CHANGE, PARTIAL_SWAP };

unsigned num_beds, num_rooms, num_features, num_departments, num_specialisms, num_patients, num_days, total_days, lower_bound, total_cost = 0;
unsigned PREFERRED_PROPERTY_WEIGHT = 20, PREFERENCE_WEIGHT = 10, SPECIALISM_WEIGHT = 20, GENDER_WEIGHT = 50, TRANSFER_WEIGHT = 100, DELAY_WEIGHT = 2, OVERCROWD_RISK_WEIGHT = 1;
unsigned MAX_CAPACITY;

char *outDir = "f:\\result.txt";
ofstream outFile(outDir, ofstream::out);

struct Rooms {
	string name;
	unsigned capacity;
	unsigned department;
	GenderPolicy policy;
};

struct Patients {
	string name;
	unsigned age;
	Gender gender;
	unsigned rday, aday, dday, tday, valid_dday;
	unsigned var;
	unsigned max_aday;
	unsigned preferred_cap;
};

struct Assignments {
	unsigned aday;
	unsigned tday;
	unsigned dday;
	unsigned ra;
	unsigned rb;
	unsigned cost;
};

vector<vector<bool> > room_property;
vector<vector<DoctoringLevel> > dept_specialism_level;
vector<vector<unsigned> > total_patient_room_cost;
vector<vector<bool> > patient_room_availability;
vector<unsigned> patient_specialism_needed;

vector<vector<Request> > patient_property_level;
vector<vector<unsigned> > patient_overlap;

vector<pair<unsigned, unsigned> > department_age_limits;
vector<unsigned> departments;
vector<unsigned> specialisms;
vector<unsigned> room_properties;

vector<Rooms*> rooms;
vector<Patients*> patients;
vector<vector<unsigned>> schedule;
vector<Assignments *> assignments;
vector<vector<unsigned>> beds;
vector<vector<unsigned>> beds_tempo;
vector<unsigned>beds_room_id;

bool prep_data(string fileName) {
	ifstream is(fileName, ios_base::in);
	if (!is.is_open())
		return false;

	string s, name, gen;
	const int BUF_SIZE = 256;
	char buf[BUF_SIZE], ch;

	unsigned p, r, pr, d, f, sp, spec; // id, val, daily_patients = 0, pi = 0,
	unsigned total_days = 0; // level, converted_level, 

							 //	Read the header
	is.getline(buf, BUF_SIZE);
	is >> s >> num_departments;
	is >> s >> num_rooms;
	is >> s >> num_features;
	is >> s >> num_patients;
	is >> s >> num_specialisms;
	is >> s >> num_days;

	num_beds = 0;

	schedule.resize(num_patients, vector<unsigned>(num_days + 1, 199));
	beds.resize(num_rooms, vector<unsigned>(num_days, NULL));
	beds_tempo.resize(num_rooms, vector<unsigned>(num_days, NULL));

	room_property.resize(num_rooms, vector<bool>(num_features, false));
	dept_specialism_level.resize(num_departments, vector<DoctoringLevel>(num_specialisms, NONE));
	department_age_limits.resize(num_departments, make_pair(0, 120));
	patient_specialism_needed.resize(num_patients);
	patient_property_level.resize(num_patients, vector<Request>(num_features, DONT_CARE));
	patient_overlap.resize(num_patients, vector<unsigned>(num_patients, 0));
	total_patient_room_cost.resize(num_patients, vector<unsigned>(num_rooms, 0));
	patient_room_availability.resize(num_patients, vector<bool>(num_rooms, true));


	//depts
	is >> ch;
	is.getline(buf, BUF_SIZE);
	for (d = 0; d < num_departments; d++) {
		is >> name >> s;
		if (s == ">=")
			is >> department_age_limits[d].first;
		else if (s == "<=")
			is >> department_age_limits[d].second;

		is >> ch; // read (
		do {
			is >> spec >> ch;
			dept_specialism_level[d][spec] = COMPLETE;
		} while (ch == ',');


		is >> ch; // read ( or -
		if (ch == '(')
			do {
				is >> spec >> ch;
				dept_specialism_level[d][spec] = PARTIAL;
			} while (ch == ',');
	}

	//rooms
	unsigned  ca, de, b, bt;
	string na, po;
	is >> ch;
	is.getline(buf, BUF_SIZE);
	for (r = 0; r < num_rooms; r++) {
		is >> na >> ca >> de >> po;

		Rooms *room = new Rooms;
		room->name = na;
		room->capacity = ca;
		room->department = de;
		if (po == "Fe") room->policy = FEMALE_ONLY;
		else if (po == "Ma") room->policy = MALE_ONLY;
		else if (po == "SG") room->policy = SAME_GENDER;
		else room->policy = TOGETHER;

		num_beds += room->capacity;

		for (d = 0; d < num_days; d++) {
			beds[r][d] = room->capacity;
		}

		is >> ch;

		if (ch == '(') {
			do {
				is >> f >> ch;
				room_property[r][f] = true;
			} while (ch != ')');
		}
		rooms.push_back(room);

	}

	beds_room_id.resize(num_beds, NULL);
	b = 0;
	bt = 0;
	for (r = 0; r < num_rooms; r++) {
		bt = bt + rooms[r]->capacity;
		for (b; b < bt; b++) {
			beds_room_id[b] = r;
		}
		b = bt;
	}


	//patients
	is >> ch;
	is.getline(buf, BUF_SIZE);

	for (p = 0; p < num_patients; p++) {
		unsigned age, entrance, leave, registration, variability, max_ad,
			treatment;
		is >> name >> age >> gen >> ch >> registration >> ch >> entrance >> ch >>
			leave >> ch >> variability >> ch >> ch;
		if (ch == '*') // no limit to the max admission
			max_ad = num_days - (leave - entrance);
		else {
			is >> ch >> max_ad;
		}
		is >> ch;

		total_days += leave - entrance;

		is >> treatment;
		patient_specialism_needed[p] = treatment;

		Patients *pat = new Patients;
		//patients.push_back(pat);
		pat->name = name;
		pat->age = age;
		pat->rday = registration;
		pat->aday = entrance;
		pat->dday = leave;
		pat->var = variability;
		pat->max_aday = max_ad;
		if (leave >= num_days) {
			pat->valid_dday = num_days;
		}
		else {
			pat->valid_dday = leave;
		}
		if (gen == "Fe") pat->gender = FEMALE;
		else pat->gender = MALE;

		is >> ch;
		if (ch == '*')
			pat->preferred_cap = MAX_CAPACITY;
		else
			is >> ch >> pat->preferred_cap;

		char lev;
		is >> ch;
		if (ch == '(') {
			do {
				is >> f >> lev >> ch;
				if (lev == 'n') // needed
					patient_property_level[p][f] = NEEDED;
				else
					patient_property_level[p][f] = PREFERRED;
			} while (ch != ')');
		}
		patients.push_back(pat);

		Assignments * assignment = new Assignments;
		assignment->aday = patients[p]->aday;
		assignment->tday = 99;
		assignment->dday = patients[p]->valid_dday;
		assignment->ra = 199;
		assignment->rb = 199;
		assignment->cost = 1000000;
		assignments.push_back(assignment);
	}


	// Compute patient-patient overlap
	for (unsigned p1 = 0; p1 < num_patients - 1; p1++)
		for (unsigned p2 = p1 + 1; p2 < num_patients; p2++) {
			for (unsigned d = patients[p1]->aday; d < patients[p1]->dday; d++)
				if (patients[p2]->aday <= d && patients[p2]->dday > d) {
					patient_overlap[p1][p2]++;
					patient_overlap[p2][p1]++;
				}
		}

	// Compute total patient room cost (and availability)

	for (p = 0; p < num_patients; p++) {
		sp = patient_specialism_needed[p];
		for (r = 0; r < num_rooms; r++) {
			// Properties
			for (pr = 0; pr < num_features; pr++) {
				if (patient_property_level[p][pr] == NEEDED && !room_property[r][pr])
					patient_room_availability[p][r] = false;
				if (patient_property_level[p][pr] == PREFERRED && !room_property[r][pr])
					total_patient_room_cost[p][r] += PREFERRED_PROPERTY_WEIGHT;
			}

			// Preferences
			if (patients[p]->preferred_cap < rooms[r]->capacity)
				total_patient_room_cost[p][r] += PREFERENCE_WEIGHT;

			// Specialism
			unsigned dep;
			dep = rooms[r]->department;
			if (dept_specialism_level[dep][sp] == PARTIAL)
				total_patient_room_cost[p][r] += SPECIALISM_WEIGHT; // * RoomDeptSpecialismLevel(r][sp] * (always 1)
			if (dept_specialism_level[dep][sp] == NONE)
				patient_room_availability[p][r] = false;

			// Department age
			if (department_age_limits[dep].first != 0 && patients[p]->age < department_age_limits[dep].first)
				patient_room_availability[p][r] = false;
			if (department_age_limits[dep].second != 0 && patients[p]->age > department_age_limits[dep].second)
				patient_room_availability[p][r] = false;

			// Gender 
			if (rooms[r]->policy == MALE_ONLY && patients[p]->gender == FEMALE)
				total_patient_room_cost[p][r] += GENDER_WEIGHT;
			if (rooms[r]->policy == FEMALE_ONLY && patients[p]->gender == MALE)
				total_patient_room_cost[p][r] += GENDER_WEIGHT;
		}
	}


	// compute lower bound
	vector<int> patient_min_cost(num_patients, -1);
	for (p = 0; p < num_patients; p++) {
		for (r = 0; r < num_rooms; r++)
			if (patient_room_availability[p][r]) {
				if (patient_min_cost[p] == -1 || total_patient_room_cost[p][r] < static_cast<unsigned>(patient_min_cost[p]))
					patient_min_cost[p] = total_patient_room_cost[p][r];
			}
		if (patient_min_cost[p] == -1) {
			cerr << "Infeasible for patient " << patients[p]->name << endl;
		}

		else
			lower_bound += static_cast<unsigned>(patient_min_cost[p]) * (patients[p]->dday - patients[p]->aday);
	}
	is >> s;
	return true;
}

// reset for restarting
void reset_schedule() {
	schedule.resize(num_patients, vector<unsigned>(num_days + 1, 199));
	for (unsigned p = 0; p < num_patients; p++) {
		schedule[p][0] = UNREGISTERED;
	}
}


// copy for restarting
void update_tempo_room_capacity() {
	unsigned r, d;
	for (r = 0; r < num_rooms; r++) {
		for (d = 0; d < num_days; d++) {
			beds_tempo[r][d] = beds[r][d];
		}
	}
}


void update_room_capacity() {
	unsigned r, d;
	for (r = 0; r < num_rooms; r++) {
		for (d = 0; d < num_days; d++) {
			beds[r][d] = beds_tempo[r][d];
		}
	}
}


bool arrange_patients(unsigned d) {
	unsigned p, a, temp, i, aday, valid_dday, ran, room;
	for (p = 0; p < num_patients; p++) {
		a = num_rooms;
		if (d == patients[p]->aday) schedule[p][0] = ADMITTED;
		else if (d == patients[p]->rday) schedule[p][0] = REGISTERED;
		else if (d == patients[p]->valid_dday) schedule[p][0] = DISCHARGED;

		if ((schedule[p][0] == ADMITTED && d == patients[p]->aday) || schedule[p][0] == REGISTERED && patients[p]->aday != patients[p]->rday) {
			aday = patients[p]->aday;
			valid_dday = patients[p]->valid_dday;
			ran = rand() % a;
			while (ran >= 0) {
				temp = 0;
				for (i = aday; i < valid_dday; i++) {
					if (beds_tempo[ran][i] < 1) break;
					temp++;
				}
				if (temp == valid_dday - aday && patient_room_availability[p][ran] == true) {
					for (i = aday; i < valid_dday; i++) {
						schedule[p][i + 1] = ran;
						beds_tempo[ran][i]--;
						//best_schedule[p][i]=schedule[p][i];
					}
					break;
				}
				else {
					if (a > 0) {
						ran = --a;
						continue;
					}
					else {
						outFile << "失败的p是" << p << endl;
						return false;
					}
				}
			}
		}
	}
	return true;
}


unsigned generate_ini_solution() {
	unsigned r, n, da, m, p, t, i, room;
	for (n = 0; n < 10000; n++) {         //用于判断是否需要重启某天的规划
		t = 1;
		reset_schedule();
		for (da = 0; da < num_days && t == 1; da++) {
			update_tempo_room_capacity();

			if (arrange_patients(da)) //所有病人成功安置
			{
				for (p = 0; p < num_patients; p++) {
					if (schedule[p][0] == REGISTERED && patients[p]->aday != patients[p]->rday) {
						for (i = patients[p]->aday; i < patients[p]->valid_dday; i++) {
							if (schedule[p][i + 1] != 199) {
								room = schedule[p][i + 1];
								beds_tempo[room][i]++;
							}
						}
					}
				}
				update_room_capacity();//更新病床余额，继续下一天的规划s
			}
			else {
				t = 0;//有人没有找到病床，重启本日的规划
			}
		}
		
		if (da = num_days - 1 && t == 1) {
			outFile << "successfully generated an initial solution!" << endl;
			break;
		}//病人全部找到病床，找到了初始解，结束本规划
	}
	return n;
}

bool calculate_cost() {
	unsigned p;
	for (p = 0; p < num_patients; p++) {
		assignments[p]->ra = schedule[p][patients[p]->aday + 1];
		assignments[p]->cost = total_patient_room_cost[p][assignments[p]->ra];
		total_cost += assignments[p]->cost;
	}
	return true;
}

/*
先用三种 Change Swap 和 delay。 思考：1. 怎么找邻居解？（找的过程用到什么参数？）
-》邻居解怎么表示？（包含参数就行）-》怎么移动？（如何更新参数）-》什么时候停止？（for循环+if语句控制次数和运行时间）
-》写入文件

现在有的:病人 patients[p]=struct patients 有各种算例中的信息和要求，床位余量 beds[room][date]=某房的剩余病床数 有各个病床的可获得性，
分配结果schedule[p][d]=room 某人在某天住哪间病房

解就是schedule向量的内容。 找邻居解就是找新的schedule。 邻居解需要储存在一个地方，比如p0在0-5住R1，无特殊要求，那一个邻居解就是p0住在
0-5均有空床的R2里。

C和PC的邻居解只需记录：谁，在哪天（为transfer预留的参数|如果是change或swap 直接等于入院日），到哪里去。
S和PS的邻居解只需记录：谁和谁，在哪天（同上），哪一段交换（如果是S则为* 不读就好|此处考虑一下：可以简化一下，比如只允许头交换）
delay的邻居解只需记录：谁，延后几天

邻居解产生若干次，用向量记录，放到向量里（二维向量）。

如果该解优于bsf解，或该解不优于bsf解但优于当前解且未被禁忌，执行change移动。schedule[p0][d0-5]=r2, beds[p0][d0-5]++.
(先后顺序比较重要，容易出错).

写入文件。

* * 注意 * * 在这里NULL会被define成0，尽量避免用NULL

*/



bool search_neighborhood_s0() {   //no 'transfer' move is allowed
	bool roomFree, samePerson = true;
	unsigned r, temp, pr, p, d, aday, valid_dday;
	//srand((unsigned)0);
	pr = rand() % num_patients;
	aday = patients[pr]->aday;
	valid_dday = patients[pr]->valid_dday;
	for (r = 0; r < num_rooms; r++) {
		for (d = aday; d < valid_dday; d++) {
			if (beds[r][d] < 1) {
				roomFree = false;
				break;
			}
			temp++;
		}
		if (temp == valid_dday - aday) {
			roomFree = true;

		}
	}
	return true;
}


bool search_neighborhood_s1() {  // 'transfer' is allowed

	return true;
}


bool execute_move() {
	//srand((unsigned)0);
	unsigned move; //para: move type
	move = 1 + rand() % 3;
	switch (move)
	{
	CHANGE:

		break;
	SWAP:
		break;
	DELAY:
		break;
	}
	return true;
}



unsigned print_solution()
{
	unsigned p, d;
	for (p = 0; p < num_patients; p++) {
		outFile << "Pat_" << p;
		outFile << " [" << schedule[p][0] << "]  ";
		for (d = 0; d < num_days; d++) {
			if (schedule[p][d + 1] != 199) {
				outFile << schedule[p][d + 1] << " ";
			}
			else {
				outFile << "-" << " ";
			}
		}
	//	outFile << total_patient_room_cost[p][assignments[p]->ra];
		outFile << endl;

	}
	//outFile << "total_cost = " <<total_cost << endl;
	return 0;
}



int main()
{
	unsigned p,li=0;
	string filename = "F:\\instance\\small_short\\small_short00.pasu";

	if (!prep_data(filename))
		cout << "数据准备失败！\n";
	srand(time(0));
	generate_ini_solution();

	//search_neighborhood();
	print_solution();
	calculate_cost();
	outFile << "Total Cost = " << total_cost << endl;
	return 0;
}



