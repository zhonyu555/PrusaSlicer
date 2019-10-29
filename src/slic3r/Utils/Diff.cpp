#include "Diff.hpp"
#include <stdlib.h>
#include <vector>

struct npArray {
	int* vals = NULL;
	int size;
	int absLower;

	npArray(int lower, int upper) {
		size = abs(lower) + abs(upper) + 1;

		vals = new int[size];
		absLower = abs(lower);
	}

	npArray(const npArray& _val) {
		size = _val.size;

		vals = new int[size];
		memcpy_s(vals, size * sizeof(int), _val.vals, size * sizeof(int));
		absLower = _val.absLower;
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

struct Snake {
	Point start;
	Point mid;
	Point end;

	Snake(int xStart, int yStart, int xMid, int yMid, int xEnd, int yEnd){
		this->start = Point(xStart, yStart);
		this->mid = Point(xMid, yMid);
		this->end = Point(xEnd, yEnd);
	}
};

namespace slic3r {
	Diff::Diff(std::string _str1, std::string _str2)
	{
		this->str1 = _str1;
		this->str2 = _str2;
	}

	void Diff::solve() {
		int len1 = str1.length();
		int len2 = str2.length();

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

			v_snapshots.emplace_back(V);
		}

		std::vector<Snake> snakes; // list to hold solution

		Point p = Point(len1, len2); // start at the end

		for (int d = v_snapshots.size() - 1; p.x > 0 || p.y > 0; d--)
		{
			npArray V = v_snapshots[d];

			int k = p.x - p.y;

			// end point is in V
			int xEnd = V[k];
			int yEnd = xEnd - k;

			// down or right?
			bool down = (k == -d || (k != d && V[k - 1] < V[k + 1]));

			int kPrev = down ? k + 1 : k - 1;

			// start point
			int xStart = V[kPrev];
			int yStart = xStart - kPrev;

			// mid point
			int xMid = down ? xStart : xStart + 1;
			int yMid = xMid - k;

			snakes.insert(snakes.begin(), Snake(xStart, yStart, xMid, yMid, xEnd, yEnd));

			p.x = xStart;
			p.y = yStart;
		}
		return;
	}
}