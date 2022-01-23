//  Created by Xan Gregg on 1/12/22.
//

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <queue>
#include <deque>
#include <array>
#include <map>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <cassert>
#include <fstream>
#include <future>
#include <thread>
#include <chrono>
#include <iomanip>
#include <thread>
#include <atomic>
#include <numeric>

using int64 = int64_t;
using namespace std;


std::chrono::time_point<std::chrono::system_clock> now() {
	return std::chrono::system_clock::now();
}

double timeDiff(const std::chrono::time_point<std::chrono::system_clock> &t0, const std::chrono::time_point<std::chrono::system_clock> &t1) {
	return std::chrono::duration<double>(t1 - t0).count();
}

double since(const std::chrono::time_point<std::chrono::system_clock> &t0) {
	return timeDiff(t0, std::chrono::system_clock::now());
}

// given a guess and the solution, return the response clue packed into a base-3 integer.
// the low-order digit is the last character.
// 0 == no match, 1 == right letter, wrong place, 2 == right letter in right place
// Using Wordle rules for duplicate letters, which are not treated the same
static int getResponse(string guess, string solution) {
	int g = 0;
	for (int i = 0; i < 5; ++i) {
		if (i > 0)
			g *= 3;
		if (guess[i] == solution[i]) {
			g += 2;
			guess[i] = ' ';
			solution[i] = ' ';
		}
	}
	int y = 0;
	for (int i = 0; i < 5; ++i) {
		if (i > 0)
			y *= 3;
		if (guess[i] == ' ')
			continue;
		auto loc = solution.find(guess[i]);
		if (loc != std::string::npos) {
			y += 1;
			solution[loc] = ' ';
		}
	}
	return g + y;
}

// given a list of possible solutions and the solutionClue data for a specific guess word,
// return a separate list for each of the 243 possible responses.
static vector<int> splitIntoPartsCount(vector<short> const &solutions, vector<short> const & solutionClue) {
	vector<int> count(243);
	for (short sol : solutions) {
		count[solutionClue[sol]] += 1;
	}
	return count;
}

// given a list of possible solutions and the solutionClue data for a specific guess word,
// return a separate list for each of the 243 possible responses.
static vector<vector<short>> splitIntoParts(vector<short> const &solutions, vector<short> const & solutionClue) {
	vector<int> count = splitIntoPartsCount(solutions, solutionClue);
	vector<vector<short>> parts(243);
	for (int i = 0; i < 243; ++i)
		parts[i].reserve(count[i]);
	for (short sol : solutions) {
		parts[solutionClue[sol]].push_back(sol);
	}
	return parts;
}

// size of the largest partition
static int splitIntoPartsMax(vector<short> const &solutions, vector<short> const & solutionClue) {
	vector<int> count = splitIntoPartsCount(solutions, solutionClue);
	return *max_element(count.begin(), count.end());
}

// read a file of words into a vector
static vector<string> readWordFile(string const & fn) {
	vector<string> words;
	ifstream wordFile(fn);
	string w;
	while (getline(wordFile, w)) {
		words.push_back(w);
	}
	wordFile.close();
	return words;
}

// show the response in a printable form, using the Wordle Unicode characters
static string responseToString(int r) {
	vector<string> code{"⬜", "\xF0\x9F\x9F\xA8", "\xF0\x9F\x9F\xA9"};   // "\uD83D\uDFE8","\uD83D\uDFE9",
	string s;
	int m = 81;
	for (int i = 0; i < 5; i++) {
		s += code[(r / m) % 3];
		m /= 3;
	}
	return s;
}

// given a guess word, w, and its response, update data on 'dead' letters by position.
// dead letters can't possible appear at a given position (position 0 is the first letter of a word)
// Doesn't have to be perfect -- just used as a hint to reduce the list of words to try.
static array<int, 5> collectDead(string w, int response, array<int, 5> const & deadLetters) {
	array<int, 5> dead = deadLetters;
	array<int, 5> code;
	int m = 81;
	for (int i = 0; i < 5; i++) {
		code[i] = (response / m) % 3;
		m /= 3;
	}
	for (int i = 0; i < 5; i++) {
		if (code[i] == 0) {
			// letter does not appear; add to dead list
			bool hasMultiple = false;	// unless it occurs multiple times in w and some are non-zero
			for (int j = 0; j < 5; ++j) {
				if (i != j && code[j] != 0 && w[i] == w[j]) {
					hasMultiple = true;
					break;
				}
			}
			if (hasMultiple)
				dead[i] |= 1 << (w[i] - 'a');	// not here
			else {
				for (int j = 0; j < 5; ++j)
					dead[j] |= 1 << (w[i] - 'a');	// not anywhere
			}
		}
		else if (code[i] == 1) {
			dead[i] |= 1 << (w[i] - 'a');	// not here
		}
	}
	return dead;
}

// decision tree
struct Path {
	int guess = -1;
	int nSolution = -1;
	int maxDepth = 0;	// worst case number of guesses, including this guess
	double ev = 0;	// expected number of guess, including this guess
	int64 solutionsPack = -1;	// up to 5 possible solutions packed 12-bits each into 64-bit int
	map<int, Path> choices;   // [243]
};

// Main work function to recursively explore and optimize the decisition tree from a given starting point.
static Path exploreGuess(vector<short> const & solution, int g, vector<vector<short>> const & solutionClue, vector<string> const & guess,
						 array<int, 5> const & deadLetters, int depthToBeat, int effort, bool shallow, int callDepth) {
	Path path;
	path.guess = g;
	path.nSolution = int(solution.size());
	if (path.nSolution <= 1) {
		// should never happen since caller tests for this (except the very first caller, which has many solutions)
		cout << "wasn't expecting <= 1 solution for " << g << endl;
		path.maxDepth = path.nSolution;
		path.ev = path.nSolution;
		if (path.nSolution > 0)
			path.solutionsPack = solution[0];
		return path;
	}
	int nGuess = int(solutionClue.size()); // ~3000

	double evsum = 0;
	double evn = 0;
	vector<vector<short>> const & parts = splitIntoParts(solution, solutionClue[g]);

	// special case for 242 == all green
	if (!parts[242].empty()) {
		evsum += 1;
		evn += 1;
		path.solutionsPack = g;	// indicates the guess is a solution
	}
	// first pass: check viability of this guess before exploring any subtree
	for (int ip = 0; ip < 242; ++ip) {
		vector<short> const & sols = parts[ip];
		if (solution.size() == sols.size()) {
			// must be a repeated or otherwise useless guess
			path.maxDepth = 999;
			return path;
		}
		if (callDepth > 0 &&
			( (solution.size() > 100 && sols.size() > solution.size() * (22 + effort * 5) / 100)
			|| (solution.size() > 75 && sols.size() > solution.size() * (33 + effort * 2) / 100)
			|| (solution.size() > 50 && sols.size() > solution.size() * (44 + effort * 2) / 100))) {
			// not a very productive guess
			path.maxDepth = 999;
			return path;
		}
	}
	// second pass: dive into each partition
	for (int ip = 0; ip < 242; ++ip) {
//		auto t0 = std::chrono::system_clock::now();
		vector<short> const & sols = parts[ip];
		if (sols.empty())
			continue;

		int nipSol = int(sols.size());
		Path best;
		if (nipSol == 1) {
			// only one solution, but it isn't guess[g] because the response is not 242
			best.guess = sols[0];
			best.maxDepth = 1;
			best.nSolution = 1;
			best.ev = best.maxDepth;
			best.solutionsPack = sols[0];
		}
		else if (nipSol == 2) {
			best.guess = sols[0];
			best.maxDepth = 2;
			best.nSolution = 2;
			best.ev = 1.5;
			best.solutionsPack = (sols[1] << 12) + sols[0];
		}
		else if (nipSol < 6) {
			// see if any solution will work
			for (int ig : sols) {
				array<int, 5> clues;
				for (int j = 0; j < nipSol; ++j) {
					clues[j] = solutionClue[ig][sols[j]];
				}
				sort(clues.begin(), clues.begin() + nipSol);
				if (std::adjacent_find(clues.begin(), clues.begin() + nipSol) == clues.begin() + nipSol) {
					best.guess = ig;
					best.nSolution = nipSol;
					best.ev = double(nipSol * 2 - 1) / nipSol;
					best.maxDepth = 2;
					best.solutionsPack = 0;
					for (int i = 0; i < min(best.nSolution, 5); ++i) {
						if (ig != sols[i])
							best.solutionsPack = (best.solutionsPack << 12) + sols[i];
					}
					best.solutionsPack = (best.solutionsPack << 12) + ig;	// put the best next guess first
					break;
				}
			}
		}
		if (best.guess >= 0) {
			// one of the above quick checks worked
		}
		else if (depthToBeat <= 3
				 || shallow || (callDepth == 1 && nipSol >= 70 + effort * 20 ) || (callDepth == 2 && nipSol > (8 + effort * 3)) || callDepth == 3) {
			// pessimistic guess
//			if (depthToBeat > 3 && callDepth == 2 && !shallow)
//				cout << "guessing" << " " << callDepth << " " << nipSol << endl;
			best.guess = sols[0];
			best.nSolution = int(sols.size());
			best.maxDepth = 3 + best.nSolution / 10;
			best.ev = 1.75 + best.nSolution / 6.0;
			best.solutionsPack = sols[0];
			for (int i = 1; i < min(best.nSolution, 5); ++i)
				best.solutionsPack = (best.solutionsPack << 12) + sols[i];
		}
		else {
			array<int, 5> deadLetters2 = collectDead(guess[g], ip, deadLetters);
			int deadLimit = (callDepth == 0 ? 0 : callDepth == 1 ? 1 : 2) + int(nipSol < 20);
			
			// first word pass: filter words bases on dead letters and also collect the max partition size for each word
			vector<pair<short, short>> maxPart;	// pair<maxPart, guess>
			for (int ig = 0; ig < nGuess; ++ig) {
				if (g == ig)
					continue;
				string const & iw = guess[ig];
				int ndead = 0;
				for (int i = 0; i < 5; ++i) {
					if ((deadLetters2[i] & (1 << (iw[i] - 'a'))) != 0) {
						ndead++;
						if (ndead > deadLimit)
							break;
					}
				}
				if (ndead > deadLimit)
					continue;
				maxPart.push_back(make_pair(short(splitIntoPartsMax(sols, solutionClue[ig])), short(ig)));
			}
			if (maxPart.empty()) {
				cout << " *** no words to consider " << g << endl;
				continue;
			}
			sort(maxPart.begin(), maxPart.end());
			int bestMax = maxPart.front().first;
			vector<int> visit;
			if (bestMax <= 2) {
				// found a winner -- just use that
				visit.push_back(maxPart.front().second);
			}
			else {
				// choose most promising words to visit fully
				int limit = bestMax * 125 / 100 + 2;
//				if (callDepth == 0) {
//					cout << guess[g] << setw(4) << ip << " " << setw(4) << bestMax << setw(4) << limit << endl;
//				}
				if (callDepth == 0) {
					// still far enough from end that we need to keep more avenues open
					limit = max(limit, nipSol / (effort == 0 ? 7 : effort == 1 ? 6 : effort <= 3 ? 5 : 4));	//5
					// HAUTE, SHADE and SPADE all have a best route with max of 36 or 35 and nipSol in 210..240 range (and bestMax is ~18)
				}
				int minVisit = clamp(int(maxPart.size()), 1, 5);
				for (auto const & mp : maxPart) {
					if (mp.first <= limit || visit.size() < minVisit)
						visit.push_back(mp.second);
					else
						break;
				}
			}
//			cout << "reduction " << g << " " << setw(4) << maxPart.size() << " " << visit.size() << " " << fixed << setprecision(2) << double(visit.size()) / double(maxPart.size()) << endl;
			vector<int> revisit;
			double bestScore = 999;
			best.maxDepth = 999;
			int minGood = effort == 0 ? 10 : effort == 1 ? 3 : 1;	// as long as maxDepth effort is light, try more for low EV
			int nGood = 0;
			for (int ig : visit) {
				Path p = exploreGuess(sols, ig, solutionClue, guess, deadLetters2, best.maxDepth, effort, shallow || callDepth == 1, callDepth + 1);
				if (p.maxDepth <= 2)
					nGood++;
				if (p.maxDepth + p.ev < bestScore) {
					bestScore = p.maxDepth + p.ev;
					best = std::move(p);
					if (best.maxDepth <= 2 && nGood >= minGood)
						break;
				}
				else if (callDepth == 1 && p.maxDepth > 3) {
					revisit.push_back(ig);
				}
			}

			if (callDepth == 1 && depthToBeat >= 4	// our best.maxDepth will be at least 3 and then add 1 before returning, so we can't beat 4
				&& best.maxDepth > 3	// 3 is ok, since the below code is not going to find a 2
				&& !shallow) {
				// another pass not changing shallow
				for (int ig : revisit) {
	//				if (callDepth <= 0) {
	//					cout << "explore " << callDepth << setw(3 * callDepth) << " " << guess[g] << " " << setw(3) << ip << " " << guess[ig] << " " << nipSol << endl;
	//				}
					Path p = exploreGuess(sols, ig, solutionClue, guess, deadLetters2, best.maxDepth, effort, shallow, callDepth + 1);
					if (p.maxDepth + p.ev < bestScore) {
						bestScore = p.maxDepth + p.ev;
						best = std::move(p);
						if (best.maxDepth <= 2)
							break;
					}
				}

			}
		}

		evsum += best.ev * best.nSolution;
		evn += best.nSolution;
		if (best.maxDepth > path.maxDepth)
			path.maxDepth = best.maxDepth;
		path.choices[ip] = best;
//		if (callDepth == 0 && sols.size() > 10) {
//			cout << ip << " " << sols.size();
//			cout << std::fixed << std::setprecision(3) << "  sec: " << " " << since(t0) << endl;
//		}
	}
	path.ev = evsum / evn + 1;
	path.maxDepth += 1;
	return path;
}


static void showPath(ofstream & treefile, string const & prefix, Path const & path, vector<string> const & guess, vector<vector<short>> const & solutionClue, int depth = 0) {
	if (path.guess < 0)
		return;
	if (path.nSolution <= 0)
		return;
	if (path.choices.empty()) {
		if (path.maxDepth <= 2 && path.nSolution <= 5 && path.solutionsPack >= 0) {
			// expand the compressed path tree
			int g = path.solutionsPack & 0x0fff;
			for (int i = 0; i < min(5, path.nSolution); ++i) {
				int s = (path.solutionsPack >> (i * 12)) & 0x0fff;
				int response = solutionClue[g][s];
				treefile << prefix << guess[g];
				if (response != 242)
					treefile << " " << responseToString(response) << std::setw(4) << 1 << "  " << guess[s];
				treefile << endl;
			}
		}
		else {
			treefile << prefix
					<< std::setw(6) << std::fixed << std::setprecision(3) << path.ev << " " << path.maxDepth;
			for (int i = 0; i < min(5, path.nSolution); ++i) {
				treefile << " ** " << guess[(path.solutionsPack >> (i * 12)) & 0x0fff];
			}
			treefile << std::endl;
		}
	}
	else {
		for (auto & kv : path.choices) {
			if (kv.second.nSolution > 0) {
				std::stringstream ss;
				ss << prefix << guess[path.guess] << " " << responseToString(kv.first) << std::setw(5) << kv.second.nSolution << "  ";
				showPath(treefile, ss.str(), kv.second, guess, solutionClue, depth + 1);
			}
		}
		if (path.solutionsPack >= 0) {	// indicates the guess is a solution
			std::stringstream ss;
			treefile << prefix << guess[path.guess] << endl;
		}
	}
}

static void pushSolutions(vector<int> & sols, Path const & path) {
	if (path.choices.empty()) {
		for (int i = 0; i < min(5, path.nSolution); ++i) {
			sols.push_back((path.solutionsPack >> (i * 12)) & 0x0fff);
		}
	}
	else {
		if (path.solutionsPack >= 0)	// indicates the guess is a solution
			sols.push_back(path.guess);
		for (auto const & kv : path.choices)
			pushSolutions(sols, kv.second);
	}
}

static void showSolutions(ostream & tableFile, Path const & path, vector<string> const & guess) {
	vector<int> sols;
	pushSolutions(sols, path);
	if (sols.size() != path.nSolution)
		tableFile << sols.size() << ";";
	for	(int i = 0; i < int(sols.size()); ++i) {
		if (i > 0)
			tableFile << ";";
		tableFile << guess[sols[i]];
	}
}


static void showTable(ostream & tableFile, Path const & path, vector<string> const & guess) {
	// "first guess,avg guesses,max guesses,first clue,n solutions,second guess,max guesses remaining,solutions"
	for (int ip = 0; ip < 243; ++ip) {
		if (path.choices.find(ip) == path.choices.end())
			continue;	// impossible response -- no solutions
		Path const & next = path.choices.at(ip);
		if (next.guess < 0 || next.nSolution == 0) {
			cout << "empty choice" << path.guess << " " << ip << endl;
			continue;	// impossible response -- no solutions
		}
		tableFile << guess[path.guess] << "," << path.ev << "," << path.maxDepth << ","
		<< responseToString(ip) << "," << next.nSolution << "," << guess[next.guess] << ","
		<< (next.nSolution == 1 && next.solutionsPack == next.guess ? 0 : next.maxDepth - 1) << ",";
		showSolutions(tableFile, next, guess);
		tableFile << endl;
	}
}

int main() {

	//vector<string> allWords = readWordFile("wordle words lines.txt");
	vector<string> solutionText = readWordFile("wordle solutions.txt");
	//solution.resize(2315);  // only the first 2315 are used as solutions
	//vector<string> guess = readWordFile("guesses5.txt");
	vector<string> guess = readWordFile("common.txt");
	sort(guess.begin(), guess.end());
	vector<short> solution;
	for (auto const & s : solutionText) {
		auto loc = find(guess.begin(), guess.end(), s);
//        assert (loc != guess.end());
		if (loc != guess.end())
			solution.push_back(int(loc - guess.begin()));
		else
			cout << "*** solution not found " << s << endl;
	}
	sort(solution.begin(), solution.end());

//    int nWords = int(words.size()); // 12972
	int nGuess = int(guess.size()); // ~3000
	int nSolution = int(solution.size());   // other words are allowed as guesses

	// encode response as 5 base-3 digits, 0 - 3^5, 0 - 243
	vector<int> place{0, 3, 9, 27, 81, 243};
	vector<vector<vector<short>>> partitions(nGuess);   // [nguess][243][nsol]
	vector<vector<short>> solutionClue(nGuess);	// [nguess][nguess] -> clue
	vector<double> score(nGuess);

	// build the response partitions for each word
	// (also compute a score for prioritizing work which is no longer used)
	auto t0 = std::chrono::system_clock::now();
	int nThread = std::clamp(int(std::thread::hardware_concurrency()), 2, 32);
	std::vector<std::thread> threads;
	for (int it = 0; it < nThread; ++it) {
		threads.emplace_back([it, nGuess, nThread, nSolution, &guess, &solution, &partitions, &solutionClue, &score]() {
			for (int ig = it * nGuess / nThread; ig < (it + 1) * nGuess / nThread; ++ig) {
				partitions[ig].resize(243);
				solutionClue[ig].resize(nGuess);
				for (short is = 0; is < nSolution; ++is) {
					int response = getResponse(guess[ig], guess[solution[is]]);
					partitions[ig][response].push_back(solution[is]);
					solutionClue[ig][solution[is]] = response;
				}
				double s = 0;
				for (int ip = 0; ip < 243; ++ip) {
					double hits = sqrt(ip % 3) + sqrt(ip/3 % 3) + sqrt(ip/9 % 3) + sqrt(ip/27 % 3) + sqrt(ip/81 % 3);
					double weight = hits == 0 ? 5.0 : hits == 1 ? 3.0 : hits < 2 ? 2.5 : hits == 2 ? 2.0 : hits < 3 ? 1.7 : 1.0;
					double ps = int(partitions[ig][ip].size()) / weight;
					if (ps > s)
						s = ps;
				}
				score[ig] = s;
			}
		} );
	}
	for (auto & t : threads)
		t.join();
	cout << std::fixed << std::setprecision(3) << "build time " << " " << since(t0) << endl;

	vector<int> index(nGuess);
	std::iota(index.begin(), index.end(), 0);
	std::sort(index.begin(), index.end(), [&](int a, int b) { return score[a] < score[b];});

	// most promising start words? -- no longer used
	for (int i = 0; i < 10; ++i) {
		int iw = index[i];
		//cout << std::right << std::setw(4) << score[iw] << " " << words[iw] << endl;
		cout << std::fixed << std::setprecision(3) << score[iw] << " " << guess[iw] << endl;
	}
	cout << endl;

#if 0
	// use this branch to try out a few specific words
	//vector<string> favWords{"trend", "begin", "route", "stein", "first", "clout", "trunk", "clone", "raise", "trend", "intro", "inert", "tinge", "avoid", "adieu"};
	//vector<string> favWords{"shush", "yummy", "mamma", "mummy", "vivid", "puppy"};
	vector<string> favWords{"shush", "yummy", "mummy"};	// 6
	vector<int> favs;
	for (auto const & s : favWords) {
		favs.push_back(int(std::find(guess.begin(), guess.end(), s) - guess.begin()));
	}
#else
	vector<int> favs;
	for (int i = 0; i < nGuess; ++i) {
		//int iw = index[i];	// in score order
		favs.push_back(i);
	}
#endif
	vector<int> favMax(favs.size());
	vector<double> favEV(favs.size());

	std::vector<stringstream> wordstreams(favs.size());
	std::vector<stringstream> summstreams(nThread);
	std::vector<std::thread> ethreads;
	std::atomic<int> next = 0;
	std::atomic<int> nDone = 0;
	std::atomic<int> nTogo = int(favs.size());
	auto te0 = std::chrono::system_clock::now();
	for (int it = 0; it < nThread; ++it) {
		ethreads.emplace_back([it, nThread, nSolution, te0, &nTogo, &nDone, &next, &favs, &guess, &solution, &partitions, &solutionClue, &score, &favMax, &favEV, &summstreams, &wordstreams]() {
			int nFav = int(favs.size());
			for (int ifav = next++; ifav < nFav; ifav = next++) {
				Path path;
				int pass = 0;
				auto tf = std::chrono::system_clock::now();
				for (; pass < 5; pass++) {
					auto tf0 = std::chrono::system_clock::now();
					if (pass > 0)
						nTogo++;
					path = exploreGuess(solution, favs[ifav], solutionClue, guess, array<int, 5>(), 999, pass, false, 0);
					favMax[ifav] = path.maxDepth;
					favEV[ifav] = path.ev;
					if (path.guess != favs[ifav]) {
						cout << "unexpected guess " << path.guess << " " << favs[ifav] << endl;
						cout.flush();
					}
					nDone++;
					
					double dt = since(te0);
					double done = double(nDone) / nTogo;	// these two atomics could be fetched out of sync but no big deal
					double rem = dt / done - dt;
					stringstream ss;
					ss << setw(3) << it << " " << setw(3) << min(4, pass) << " " << setw(4) << ifav << "  " << guess[favs[ifav]] << "  "
					<< std::fixed << std::setprecision(5) << favEV[ifav] << std::right << std::setw(6) << favMax[ifav] << " "
					<< setw(7) << std::setprecision(2) << since(tf0)/60 << " min     "
					<< setw(5) << std::setprecision(1) << done * 100 << "% done  "
					<< setw(5) << std::setprecision(0) << rem/60 << " min to go"<< endl;
					cout << ss.str();
					
					// *** "results" directory needs to already exist ***
					
					// the entire decision tree for this starting word
					ofstream treefile;
					treefile.open(std::filesystem::path(string("results/tree ")
														+ (path.maxDepth > 5 && pass < 4 ? (std::to_string(pass) + " ") : string(""))
														+ guess[favs[ifav]] + ".txt"));
					showPath(treefile, "", path, guess, solutionClue);
					treefile.close();
					
					if (path.maxDepth <= 5)
						break;	// good enough
				}

				showTable(wordstreams[ifav], path, guess);
				
				summstreams[it] << guess[favs[ifav]] << "," << std::fixed << std::setprecision(5) << path.ev << "," << path.maxDepth << "," << pass << "," << since(tf) << endl;
			}
		} );
	}
	for (auto & t : ethreads)
		t.join();
	for (int ifav = 0; ifav < int(favs.size()); ++ifav) {
		cout << guess[favs[ifav]] << " " << std::fixed << std::setw(6) << std::setprecision(3) << favEV[ifav] << std::right << std::setw(6) << favMax[ifav] << endl;
	}
	
	ofstream summFile;
	summFile.open(std::filesystem::path(string("results/summary ev.csv")));
	summFile << "guess" << "," << "ev" << "," << "max guesses" << "," << "pass" << "," << "time" << endl;
	for (auto & s : summstreams) {
		summFile << s.str();
	}
	summFile.close();

	ofstream twoDeepFile;
	twoDeepFile.open(std::filesystem::path(string("results/summary two-deep.csv")));
	twoDeepFile << "first guess,avg guesses,max guesses,first clue,n solutions,second guess,max guesses remaining,solutions" << endl;
	for (auto & s : wordstreams) {
		twoDeepFile << s.str();
	}
	twoDeepFile.close();

	cout << endl;
	return 0;
}
