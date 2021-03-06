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

#ifndef OPENING_BOOK_HPP
#define OPENING_BOOK_HPP

#include <iostream>
#include <fstream>
#include "Position.hpp"
#include "TranspositionTable.hpp"

namespace GameSolver {
namespace Connect4 {

class OpeningBook {
    TableGetter<uint8_t> *T;
    const int width;
    const int height;
    int depth;

    template<class partial_key_t>
    TableGetter<uint8_t>* initTranspositionTable(int log_size) {
        switch(log_size) {
        case 14:
            return new TranspositionTable<partial_key_t, uint8_t, 14>();
        case 21:
            return new TranspositionTable<partial_key_t, uint8_t, 21>();
        case 22:
            return new TranspositionTable<partial_key_t, uint8_t, 22>();
        case 23:
            return new TranspositionTable<partial_key_t, uint8_t, 23>();
        case 24:
            return new TranspositionTable<partial_key_t, uint8_t, 24>();
        case 25:
            return new TranspositionTable<partial_key_t, uint8_t, 25>();
        case 26:
            return new TranspositionTable<partial_key_t, uint8_t, 26>();
        case 27:
            return new TranspositionTable<partial_key_t, uint8_t, 27>();
        default:
            std::cerr << "Unimplemented OpeningBook size: " << log_size << std::endl;
            return 0;
        }
    }

    TableGetter<uint8_t>* initTranspositionTable(int key_bytes, int log_size) {
        switch(key_bytes) {
        case 1:
            return initTranspositionTable<uint8_t>(log_size);
        case 2:
            return initTranspositionTable<uint16_t>(log_size);
        case 4:
            return initTranspositionTable<uint32_t>(log_size);
        default:
            std::cerr << "Invalid key size: " << key_bytes << " bytes" << std::endl;
            return 0;
        }
    }

public:
    OpeningBook(int width, int height) : T{0}, width{width}, height{height}, depth{ -1} {} // Empty opening book

    OpeningBook(int width, int height, int depth, TableGetter<uint8_t>* T) : T{T}, width{width}, height{height}, depth{depth} {} // Empty opening book

    /**
     * Empty the book
     */
    void clear() {
        if (T) T->reset();
    }

    /**
     * Opening book file format:
     * - 1 byte: board width
     * - 1 byte: board height
     * - 1 byte: max stored position depth
     * - 1 byte: key size in bits
     * - 1 byte: value size in bits
     * - 1 byte: log_size = log2(size). number of stored elements (size) is smallest prime number above 2^(log_size)
     * - size key elements
     * - size value elements
     */
    void load(std::string filename, bool show = false) {
        depth = -1;
        delete T;
        std::ifstream ifs(filename, std::ios::binary); // open file

        if(ifs.fail()) {
            std::cerr << "Unable to load opening book: " << filename << std::endl;
            return;
        } // else std::cerr << "Loading opening book from file: " << filename << ". ";

        char _width, _height, _depth, value_bytes, key_bytes, log_size;

        ifs.read(&_width, 1);
        if(ifs.fail() || _width != width) {
            std::cerr << "Unable to load opening book: invalid width (found: " << int(_width) << ", expected: " << width << ")" << std::endl;
            return;
        }

        ifs.read(&_height, 1);
        if(ifs.fail() || _height != height) {
            std::cerr << "Unable to load opening book: invalid height(found: " << int(_height) << ", expected: " << height << ")"  << std::endl;
            return;
        }

        ifs.read(&_depth, 1);
        if(ifs.fail() || _depth > width * height) {
            std::cerr << "Unable to load opening book: invalid depth (found: " << int(_depth) << ")"  << std::endl;
            return;
        }

        ifs.read(&key_bytes, 1);
        if(ifs.fail() || key_bytes > 8) {
            std::cerr << "Unable to load opening book: invalid key size(found: " << int(key_bytes) << ")"  << std::endl;
            return;
        }

        ifs.read(&value_bytes, 1);
        if(ifs.fail() || value_bytes != 1) {
            std::cerr << "Unable to load opening book: invalid value size (found: " << int(value_bytes) << ", expected: 1)"  << std::endl;
            return;
        }

        ifs.read(&log_size, 1);
        if(ifs.fail() || log_size > 40) {
            std::cerr << "Unable to load opening book: invalid log2(size)(found: " << int(log_size) << ")"  << std::endl;
            return;
        }

        if((T = initTranspositionTable(key_bytes, log_size))) {
            ifs.read(reinterpret_cast<char *>(T->getKeys()), T->getSize() * key_bytes);
            ifs.read(reinterpret_cast<char *>(T->getValues()), T->getSize() * value_bytes);
            if(ifs.fail()) {
                std::cerr << "Unable to load data from opening book" << std::endl;
                return;
            }
            depth = _depth; // set it in case of success only, keep -1 in case of failure
            //std::cerr << "done" << std::endl;
        }
        else std::cerr << "Unable to initialize opening book" << std::endl;
        ifs.close();

        if (show) {
            std::cout << "Loaded book from file: " << filename << "\n";
            std::cout << "  width      : " << width << "\n";
            std::cout << "  height     : " << height << "\n";
            std::cout << "  depth      : " << depth << "\n";
            std::cout << "  key size   : " << int(key_bytes) << "\n";
            std::cout << "  value size : " << int(value_bytes) << "\n";
            std::cout << "  log size   : " << int(log_size) << "\n";
        }
    }

    void save(const std::string output_file) const {
        if (T) {
            std::ofstream ofs(output_file, std::ios::binary);
            char tmp;
            tmp = width;
            ofs.write(&tmp, 1);
            tmp = height;
            ofs.write(&tmp, 1);
            tmp = depth;
            ofs.write(&tmp, 1);
            tmp = T->getKeySize();
            ofs.write(&tmp, 1);
            tmp = T->getValueSize();
            ofs.write(&tmp, 1);
            tmp = log2(T->getSize());
            ofs.write(&tmp, 1);

            ofs.write(reinterpret_cast<const char *>(T->getKeys()), T->getSize() * T->getKeySize());
            ofs.write(reinterpret_cast<const char *>(T->getValues()), T->getSize() * T->getValueSize());
            ofs.close();
        }
    }

    int get(const Position &P) const {
        if(P.nbMoves() > depth) return 0;

        return T ? T->get(P.key3()) : 0;
    }

    ~OpeningBook() {
        delete T;
    }
};

} // namespace Connect4
} // namespace GameSolver
#endif
