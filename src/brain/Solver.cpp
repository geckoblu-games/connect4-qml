/*
 * This file is part of Connect4 Game Solver <http://connect4.gamesolver.org>
 * Copyright (C) 2017-2019 Pascal Pons <contact@gamesolver.org>
 *
 * Connect4 Game Solver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Connect4 Game Solver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Connect4 Game Solver. If not, see <http://www.gnu.org/licenses/>.
 */

#include <cassert>
#include "Solver.hpp"
#include "MoveSorter.hpp"
#include "MoveChooser.hpp"

using namespace GameSolver::Connect4;

namespace GameSolver {
namespace Connect4 {

/**
 * Reccursively score connect 4 position using negamax variant of alpha-beta algorithm.
 * @param: position to evaluate, this function assumes nobody already won and
 *         current player cannot win next move. This has to be checked before
 * @param: alpha < beta, a score window within which we are evaluating the position.
 *         depth, the depth it should search for (-1 infinite)
 *
 * @return the exact score, an upper or lower bound score depending of the case:
 * - if actual score of position <= alpha then actual score <= return value <= alpha
 * - if actual score of position >= beta then beta <= return value <= actual score
 * - if alpha <= actual score <= beta then return value = actual score
 */
int Solver::negamax(const Position &P, int alpha, int beta, int depth) {
    assert(alpha < beta);
    assert(!P.canWinNext());

    uint64_t possible = P.possibleNonLosingMoves();
    if(possible == 0)     // if no possible non losing move, opponent wins next move
        return -(Position::WIDTH * Position::HEIGHT - P.nbMoves()) / 2;

    if(P.nbMoves() >= Position::WIDTH * Position::HEIGHT - 2) // check for draw game
        return 0;

    int min = -(Position::WIDTH * Position::HEIGHT - 2 - P.nbMoves()) / 2;	// lower bound of score as opponent cannot win next move
    if(alpha < min) {
        alpha = min;                     // there is no need to keep alpha below our max possible score.
        if(alpha >= beta) return alpha;  // prune the exploration if the [alpha;beta] window is empty.
    }

    int max = (Position::WIDTH * Position::HEIGHT - 1 - P.nbMoves()) / 2;	// upper bound of our score as we cannot win immediately
    if(beta > max) {
        beta = max;                     // there is no need to keep beta above our max possible score.
        if(alpha >= beta) return beta;  // prune the exploration if the [alpha;beta] window is empty.
    }

    const uint64_t key = P.key();
    if(int val = transTable.get(key)) {
        if(val > Position::MAX_SCORE - Position::MIN_SCORE + 1) { // we have an lower bound
            min = val + 2 * Position::MIN_SCORE - Position::MAX_SCORE - 2;
            if(alpha < min) {
                alpha = min;                     // there is no need to keep beta above our max possible score.
                if(alpha >= beta) return alpha;  // prune the exploration if the [alpha;beta] window is empty.
            }
        } else { // we have an upper bound
            max = val + Position::MIN_SCORE - 1;
            if(beta > max) {
                beta = max;                     // there is no need to keep beta above our max possible score.
                if(alpha >= beta) return beta;  // prune the exploration if the [alpha;beta] window is empty.
            }
        }
    }

    if(int val = book.get(P)) return val + Position::MIN_SCORE - 1; // look for solutions stored in opening book

    if (depth == 0) {
        return 0; // TODO: I'm not sure about this
    } else {
        depth > 0 ? --depth : depth; // if depth < 0 go to infinite depth
    }

    MoveSorter moves;
    for(int i = Position::WIDTH; i--;)
        if(uint64_t move = possible & Position::column_mask(columnOrder[i]))
            moves.add(move, P.moveScore(move));

    while(uint64_t next = moves.getNext()) {
        Position P2(P);
        P2.play(next);  // It's opponent turn in P2 position after current player plays x column.
        int score = -negamax(P2, -beta, -alpha, depth); // explore opponent's score within [-beta;-alpha] windows:
        // no need to have good precision for score better than beta (opponent's score worse than -beta)
        // no need to check for score worse than alpha (opponent's score worse better than -alpha)

        if(score >= beta) {
            transTable.put(key, score + Position::MAX_SCORE - 2 * Position::MIN_SCORE + 2); // save the lower bound of the position
            return score;  // prune the exploration if we find a possible move better than what we were looking for.
        }
        if(score > alpha) alpha = score; // reduce the [alpha;beta] window for next exploration, as we only
        // need to search for a position that is better than the best so far.
    }

    transTable.put(key, alpha - Position::MIN_SCORE + 1); // save the upper bound of the position
    return alpha;
}

int Solver::solve(const Position &P, int depth, bool weak) {
    if(P.canWinNext()) // check if win in one move as the Negamax function does not support this case.
        return (Position::WIDTH * Position::HEIGHT + 1 - P.nbMoves()) / 2;
    int min = -(Position::WIDTH * Position::HEIGHT - P.nbMoves()) / 2;
    int max = (Position::WIDTH * Position::HEIGHT + 1 - P.nbMoves()) / 2;
    if(weak) {
        min = -1;
        max = 1;
    }

    while(min < max) {                    // iteratively narrow the min-max exploration window
        int med = min + (max - min) / 2;
        if(med <= 0 && min / 2 < med) med = min / 2;
        else if(med >= 0 && max / 2 > med) med = max / 2;
        int r = negamax(P, med, med + 1, depth);   // use a null depth window to know if the actual score is greater or smaller than med
        if(r <= med) max = r;
        else min = r;
    }
    return min;
}

static int getMoveColumn(uint64_t move) {
    for(int i = Position::WIDTH; i--;) {
        if(move & Position::column_mask(i)) {
            return i;
        }
    }
    assert(false && "Unable to find a column for the move"); // I should never be here
}

int Solver::getBestMove(const Position &P, int depth, bool weak) {
    uint64_t possible = P.possible();
    if(possible == 0) {
        return -1;
    }

    std::cerr << "-------\n";

    MoveSorter moves;
    for(int i = Position::WIDTH; i--;)
        if(uint64_t move = possible & Position::column_mask(columnOrder[i]))
            moves.add(move, P.moveScore(move));

    MoveChooser chooser;
    while(uint64_t next = moves.getNext()) {
        Position P2(P);
        P2.play(next);
        int score = -solve(P2, depth, weak);
        std::cerr << "next: " << getMoveColumn(next) << "  score: " << score << "\n";
        chooser.add(next, score);
    }

    std::cerr << "-------\n";

    int best =  getMoveColumn(chooser.getBestMove());
    std::cerr << "best: " << best << "  score: " << chooser.getBestScore() << "\n";
    std::cerr << "-------\n";

    return best;
}

// Constructor
Solver::Solver() {
    for(int i = 0; i < Position::WIDTH; i++) // initialize the column exploration order, starting with center columns
        columnOrder[i] = Position::WIDTH / 2 + (1 - 2 * (i % 2)) * (i + 1) / 2; // example for WIDTH=7: columnOrder = {3, 4, 2, 5, 1, 6, 0}
}

} // namespace Connect4
} // namespace GameSolver
