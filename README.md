# deepwordle

Results from a searching for optimal decision trees for given starting words. "Optimal" meaning having the best worst case performance, which for almost alll words is five guesses. Only three words have been found to need six guesses: MUMMY, SLUSH and YUMMY.

"Deep" is in constrast to most optimization approaches on Wordle data which use greedy approaches, which are "shallow" with respect to the decision tree.

### common.txt
A list of my collated "common" five letter English words. Combines the Wordle solutions with other lists and adds plurals and past tense words.

### summary ev.csv
Summary results for each common word as a starting guess
* guess: the word
* ev: the average depth of solutions in the guess's optimal decision tree (ev = expected value)
* max guesses: maximum depth of the guess's decision tree
* pass: interval diagnostic, how many passes were needed to find a depth-five tree (each pass words harder)
* time: interval diagnostic, time in seconds to find the decision tree

### results directory
ZIP files of decision tree text files. 
