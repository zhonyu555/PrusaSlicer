#include "Diff.hpp"
#include <stdlib.h>

#ifdef _DEBUG
#include <algorithm>
#include <cassert>
#endif

struct npArray {
	int* vals = NULL;
	int size;
	int absLower;

	npArray(int lower, int upper) {
		size = abs(lower) + upper + 1;

		vals = new int[size];
		absLower = abs(lower);
	}

	npArray(const npArray& _val) {
		size = _val.size;

		vals = new int[size];
		absLower = _val.absLower;
		memcpy_s(vals, size * sizeof(int), _val.vals, size * sizeof(int));
	}

	npArray(const npArray& _val, int lower, int upper) : npArray(lower, upper) {
		memcpy_s(vals, size * sizeof(int), &(_val.vals[lower + _val.absLower]), size * sizeof(int));
	}

	~npArray() {
		delete [] vals;
	}

	int& operator[](int index) {
		return vals[index + this->absLower];
	}
};

struct Point {
	int x = 0;
	int y = 0;

	Point(){}

	Point(int _x, int _y) {
		x = _x;
		y = _y;
	}
};

namespace slic3r {
	const std::string& Diff::EditScriptAction::get_target_str(const std::string& str1, const std::string& str2) {
		if (this->actionType == Diff::EditScriptAction::ActionType::keep || this->actionType == Diff::EditScriptAction::ActionType::remove) {
			return str1;
		}
		return str2;
	}

	Diff::EditScript Diff::EditScriptAction::split_by_newline(const std::string& str1, const std::string& str2) {
		std::vector<Diff::EditScriptAction> acts;

		const std::string& str = get_target_str(str1, str2);

		size_t cur_off = this->offset;
		size_t end_off = cur_off + this->count;

		//0123456789
		//1n2n3n4n
		//-------	keep
		//       -	add

		bool cont = true;
		int lb_type = this->actionType + Diff::EditScriptAction::ActionType::lineBreakType;

		while (cont && cur_off < end_off) {
			size_t pos = str.find("\n", cur_off);

			if (pos == std::string::npos || pos >= end_off) {
				pos = end_off;
				cont = false;
			}

			acts.emplace_back(pos == cur_off ? lb_type : this->actionType, cur_off, pos - cur_off);
			if (cont) {
				if (pos != cur_off) {
					acts.emplace_back(lb_type, 0, 0);
				}
				cur_off = pos + 1;
			}
		}

		return acts;
	}

	Diff::Diff(const std::string& _str1, const std::string& _str2, bool split_newline)
	{
		solve(_str1, _str2, split_newline);
	}

	void Diff::solve(const std::string& str1, const std::string& str2, bool split_newline) {
		if (str1.empty() && str2.empty()) {
			return;
		}

		int len1 = (int)str1.length();
		int len2 = (int)str2.length();

		npArray V = npArray(-(len1 + len2), len1 + len2);
		V[1] = 0;
		bool cont = true;
		std::vector<npArray> v_snapshots; // saved V's indexed on d

		for (int d = 0; d <= len1 + len2 && cont; d++)
		{
			for (int k = -d; k <= d; k += 2)
			{
				// down or right?
				bool down = (k == -d || (k != d && V[k - 1] < V[k + 1]));

				int kPrev = down ? k + 1 : k - 1;

				// start point
				int xStart = V[kPrev];
				int yStart = xStart - kPrev;

				// mid point
				int xMid = down ? xStart : xStart + 1;
				int yMid = xMid - k;

				// end point
				int xEnd = xMid;
				int yEnd = yMid;

				// follow diagonal
				int snake = 0;
				while (xEnd < len1 && yEnd < len2 && str1[xEnd] == str2[yEnd]) { xEnd++; yEnd++; snake++; }

				// save end point
				V[k] = xEnd;

				// check for solution
				if (xEnd >= len1 && yEnd >= len2) {
					/* solution has been found */
					cont = false;
					break;
				}
			}

			v_snapshots.emplace_back(V, -d, d);
		}

		Point p = Point(len1, len2); // start at the end

		for (int d = (int)(v_snapshots.size() - 1); p.x > 0 || p.y > 0; d--)
		{
			npArray V = v_snapshots[d];

			int k = p.x - p.y;

			// end point is in V
			int xEnd = V[k];
			int yEnd = xEnd - k;

			// down or right?
			bool down = (k == -d || (k != d && V[k - 1] < V[k + 1]));

			int kPrev = down ? k + 1 : k - 1;

			int xStart, yStart, xMid, yMid;
			if (d == 0) {
				xStart = 0;
				yStart = -1;

				xMid = 0;
				yMid = 0;
			}
			else {
				xStart = V[kPrev];
				yStart = xStart - kPrev;

				// mid point
				xMid = down ? xStart : xStart + 1;
				yMid = xMid - k;
			}

			if (xEnd != xMid) { //keep from A
				EditScriptAction act = EditScriptAction(EditScriptAction::ActionType::keep, xMid, (size_t)xEnd - xMid);
				solution.insert(solution.begin(), act);
			}

			if (down) { //insert from B
				if (yMid > 0) { //ignore the stub starting point (V[1]=0)
					size_t off = (size_t)yMid - 1;

					if (solution.size() > 0 && solution[0].actionType == EditScriptAction::ActionType::insert) {
						solution[0].offset = off;
						solution[0].count++;
					}
					else {
						EditScriptAction act = EditScriptAction(EditScriptAction::ActionType::insert, off, 1);
						solution.insert(solution.begin(), act);
					}
				}
			}
			else { //remove from A
				size_t off = (size_t)xMid - 1;

				if (solution.size() > 0 && solution[0].actionType == EditScriptAction::ActionType::remove) {
					solution[0].offset = off;
					solution[0].count++;
				}
				else {
					EditScriptAction act = EditScriptAction(EditScriptAction::ActionType::remove, off, 1);
					solution.insert(solution.begin(), act);
				}
			}

			p.x = xStart;
			p.y = yStart;
		}

		if (split_newline) {
			EditScript new_sol;

			for (EditScriptAction& cur_action : solution) {
				EditScript acts = cur_action.split_by_newline(str1, str2);
				new_sol.insert(new_sol.end(), acts.begin(), acts.end());
			}

			solution = EditScript(std::move(new_sol));
		}
	}

	const Diff::EditScript& Diff::getSolution() {
		return this->solution;
	}

#ifdef _DEBUG
	void Diff::selfTest(int count) {
		auto ranStr = [](size_t length) -> std::string
		{
			auto randchar = []() -> char
			{
				const char charset[] =
					"0123456789"
					"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					"abcdefghijklmnopqrstuvwxyz";
				const size_t max_index = (sizeof(charset) - 1);
				return charset[rand() % max_index];
			};
			std::string str(length, 0);
			std::generate_n(str.begin(), length, randchar);
			return str;
		};

		for (int i = 0; i < count; i++) {
			std::string s1, s2;
			switch (i) {
				case 0: {
					s1 = "";
					s2 = "";
					break;
				}
				case 1: {
					s1 = "";
					s2 = "Awmaaa";
					break;
				}
				case 2: {
					s1 = "aweowae";
					s2 = "";
					break;
				}
				default: {
					 s1 = ranStr(rand() % 200);
					 s2 = ranStr(rand() % 200);
				}
			}

			Diff diff = Diff(s1, s2);

			std::string testStr = "";

			for (EditScriptAction cur_action : diff.getSolution()) {
				switch (cur_action.actionType)
				{
					case EditScriptAction::ActionType::insert: {
						testStr += s2.substr(cur_action.offset, cur_action.count) ;
						break;
					}
					case EditScriptAction::ActionType::keep: {
						testStr += s1.substr(cur_action.offset, cur_action.count);
						break;
					}
				}
			}

			assert(testStr == s2);
		}
	}
#endif
}