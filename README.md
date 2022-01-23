# deepwordle

A companion blog post is on [rawdatastudies.com](https://rawdatastudies.com/2022/01/22/deep-wordle/).
Results from a searching for optimal decision trees for given starting words. "Optimal" meaning having
the best worst case performance, which for almost alll words is five guesses. Only three words have been
found to need six guesses: MUMMY, SLUSH and YUMMY.

"Deep" is in constrast to most optimization approaches on Wordle data which use greedy approaches, which are "shallow" with respect to the decision tree.

### common.txt
A list of my collated "common" five letter English words. Combines the Wordle solutions with other lists and adds plurals and past tense words.

### solveInFive.cpp
C++ code for generating decision trees and summary info. Assumes a "results" directory exists in the default directory.
Takes about 10 hours to process all 3622 common words on my 10-core machine, but you can choose a subset of starting words.

### summary ev.csv
Summary results for each common word as a starting guess
* guess: the word
* ev: the average depth of solutions in the guess's optimal decision tree (ev = expected value)
* max guesses: maximum depth of the guess's decision tree
* pass: internal diagnostic, how many passes were needed to find a depth-five tree (each pass works harder)
* time: internal diagnostic, time in seconds to find the decision tree

### results directory
ZIP files of a summary of "two-deep" choices and individual decision tree text files.
Each "tree abcde.txt" file has the decision tree for a single word (abcde).
Each has 2315 lines, one line per Wordle solution word.
The format was meant to be human-readable but is regular enough for machine processing as spaces-delimited fields.
(Parsing as fixed-width may work, depending on how your parser counts the Unicode characters.)
Each line is a sequence of triplets consisting of:
* guess, the five letter word
* response, the Wordle formatted response
* count, the number of words satisfying that response

However, for the last triplet, only the first field is present since its response and count are necessarily 🟩🟩🟩🟩🟩 and 1. In the sample below, TIGER is the initial guess. For the response of no hits there are 321 valid solutions and CLASP is the best next guess. For a response of just the last letter yellow, there are 9 valid solutions. From there DUMPY is the best guess as it uniquely identifies the solution, which might be DUMPY itself.

tiger ⬜⬜⬜⬜⬜ 321  clasp ⬜⬜⬜⬜🟨   9  dumpy ⬜⬜⬜🟨🟩   1  phony\
tiger ⬜⬜⬜⬜⬜ 321  clasp ⬜⬜⬜⬜🟨   9  dumpy ⬜⬜⬜🟩🟩   1  poppy\
tiger ⬜⬜⬜⬜⬜ 321  clasp ⬜⬜⬜⬜🟨   9  dumpy ⬜⬜🟩🟩🟨   1  nymph\
tiger ⬜⬜⬜⬜⬜ 321  clasp ⬜⬜⬜⬜🟨   9  dumpy ⬜🟩⬜🟨🟩   1  puffy\
tiger ⬜⬜⬜⬜⬜ 321  clasp ⬜⬜⬜⬜🟨   9  dumpy ⬜🟩⬜🟩🟩   1  puppy\
tiger ⬜⬜⬜⬜⬜ 321  clasp ⬜⬜⬜⬜🟨   9  dumpy ⬜🟩🟩🟩⬜   1  humph\
tiger ⬜⬜⬜⬜⬜ 321  clasp ⬜⬜⬜⬜🟨   9  dumpy ⬜🟩🟩🟩🟩   1  jumpy\
tiger ⬜⬜⬜⬜⬜ 321  clasp ⬜⬜⬜⬜🟨   9  dumpy 🟨🟨⬜🟨⬜   1  pound\
tiger ⬜⬜⬜⬜⬜ 321  clasp ⬜⬜⬜⬜🟨   9  dumpy\
tiger ⬜⬜⬜⬜⬜ 321  clasp ⬜⬜⬜⬜🟩   1  whoop\

