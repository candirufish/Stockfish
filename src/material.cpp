/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2018 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm> // For std::min
#include <cassert>
#include <cstring>   // For std::memset

#include "material.h"
#include "thread.h"

using namespace std;

namespace {

  // Polynomial material imbalance parameters

	constexpr int Linear[] = { 1499, 6, -9, 2, 18, 5 };

	constexpr int QuadraticOurs[][PIECE_TYPE_NB] = {
    //            OUR PIECES
    // pair pawn knight bishop rook queen
    {   2                                 }, // Bishop pair
    {  41,    3                           }, // Pawn
    {  35,  248,   -1                     }, // Knight      OUR PIECES
    {  -4,  121,    2,     0              }, // Bishop
    { -23,   -2,   49,    92,  -131       }, // Rook
    {-187,   28,  127,   125,  -142, -12  }  // Queen
  };

	constexpr int QuadraticTheirs[][PIECE_TYPE_NB] = {
    //           THEIR PIECES
    // pair pawn knight bishop rook queen
    {   2                                }, // Bishop pair
    {  45,    3                          }, // Pawn
    {   10,   63,   0                     }, // Knight      OUR PIECES
    {  56,   75,  44,     2              }, // Bishop
    {  48,   38,  27,   -28,   1        }, // Rook
    { 108,   94, -31,   140,  212,   -3  }  // Queen
  };

	constexpr int CubicOurs[][PIECE_TYPE_NB][PIECE_TYPE_NB] = {
    // OUR PIECES:
    {}, // Bishop pair
    {   // Pawn
        //            OUR PIECES
        // pair pawn knight bishop rook queen
        {   -7                               }, // Bishop pair
        {    2,   -10                        }  // Pawn        OUR PIECES
    },
    {   // Knight
        //            OUR PIECES
        // pair pawn knight bishop rook queen
        {    0                               }, // Bishop pair
        {    -4,   8                         }, // Pawn
        {    9,   -7,   2                   }  // Knight      OUR PIECES
    },
    {   // Bishop
        //            OUR PIECES
        // pair pawn knight bishop rook queen
        {   -5                               }, // Bishop pair
        {   -5,    2                         }, // Pawn
        {   -3,   -7,    10                   }, // Knight      OUR PIECES
        {    1,    5,    3,   -8             }  // Bishop
    },
    {   // Rook
        //            OUR PIECES
        // pair pawn knight bishop rook queen
        {   -2                               }, // Bishop pair
        {    0,   8                         }, // Pawn
        {   -4,   -6,    4                   }, // Knight      OUR PIECES
        {    2,    3,    1,   -3             }, // Bishop
        {   -8,   -5,   -5,   -5,   -5       }  // Rook
    },
    {   // Queen
        //            OUR PIECES
        // pair pawn knight bishop rook queen
        {    2                               }, // Bishop pair
        {   0,   -6                         }, // Pawn
        {   -5,  -14,    -4                   }, // Knight      OUR PIECES
        {    1,    3,   -3,   -1             }, // Bishop
        {   -4,    0,   1,    -1,   3       }, // Rook
        {    0,    9,   -4,    -3,    8,    2 }  // Queen
    }
  };

	constexpr int CubicTheirs[][PIECE_TYPE_NB][PIECE_TYPE_NB] = {
    // OUR PIECES:
    {}, // Bishop pair
    {   // Pawn
        //            THEIR PIECES
        // pair pawn knight bishop rook queen
        {   -2,   -3,   -3,   -5,    1,    6 }, // Bishop pair
        {   -2,   21,    -4,    0,    6,   -8 }  // Pawn        OUR PIECES
    },
    {   // Knight
        //            THEIR PIECES
        // pair pawn knight bishop rook queen
        {   0,    8,    1,    5,   -2,    0 }, // Bishop pair
        {   -12,   1,    0,   -7,    -2,    3 }, // Pawn
        {   16,    5,   -4,    -2,   0,   -11 }  // Knight      OUR PIECES
    },
    {   // Bishop
        //            THEIR PIECES
        // pair pawn knight bishop rook queen
        {   -4,   -2,    7,    -4,    5,   0 }, // Bishop pair
        {   1,    0,   -6,    4,    5,   -4 }, // Pawn
        {    4,    3,    3,   -6,    0,   -5 }, // Knight      OUR PIECES
        {    8,    2,   -2,    0,   -2,   -2 }  // Bishop
    },
    {   // Rook
        //            THEIR PIECES
        // pair pawn knight bishop rook queen
        {    10,    1,    4,    4,    11,   -3 }, // Bishop pair
        {    -8,   -7,    5,    8,    4,    4 }, // Pawn
        {    5,    4,    1,   0,    1,    6 }, // Knight      OUR PIECES
        {    5,    -5,    2,    -3,    1,   -6 }, // Bishop
        {    -1,    4,   -1,   -1,   -3,    0 }  // Rook
    },
    {   // Queen
        //            THEIR PIECES
        // pair pawn knight bishop rook queen
        {    6,   4,    2,    -4,   -3,   1 }, // Bishop pair
        {    -6,    4,    1,    3,   -5,    8 }, // Pawn
        {    1,   7,   0,    0,   -1,    2 }, // Knight      OUR PIECES
        {   -7,    5,    2,    -3,    2,    -9 }, // Bishop
        {   -3,    8,   -4,   -4,    2,   -2 }, // Rook
        {   -6,    0,   4,   1,   -6,    7 }  // Queen
    }
  };
  
  // Endgame evaluation and scaling functions are accessed directly and not through
  // the function maps because they correspond to more than one material hash key.
  Endgame<KXK>    EvaluateKXK[] = { Endgame<KXK>(WHITE),    Endgame<KXK>(BLACK) };

  Endgame<KBPsK>  ScaleKBPsK[]  = { Endgame<KBPsK>(WHITE),  Endgame<KBPsK>(BLACK) };
  Endgame<KQKRPs> ScaleKQKRPs[] = { Endgame<KQKRPs>(WHITE), Endgame<KQKRPs>(BLACK) };
  Endgame<KPsK>   ScaleKPsK[]   = { Endgame<KPsK>(WHITE),   Endgame<KPsK>(BLACK) };
  Endgame<KPKP>   ScaleKPKP[]   = { Endgame<KPKP>(WHITE),   Endgame<KPKP>(BLACK) };

  // Helper used to detect a given material distribution
  bool is_KXK(const Position& pos, Color us) {
    return  !more_than_one(pos.pieces(~us))
          && pos.non_pawn_material(us) >= RookValueMg;
  }

  bool is_KBPsK(const Position& pos, Color us) {
    return   pos.non_pawn_material(us) == BishopValueMg
          && pos.count<BISHOP>(us) == 1
          && pos.count<PAWN  >(us) >= 1;
  }

  bool is_KQKRPs(const Position& pos, Color us) {
    return  !pos.count<PAWN>(us)
          && pos.non_pawn_material(us) == QueenValueMg
          && pos.count<QUEEN>(us)  == 1
          && pos.count<ROOK>(~us) == 1
          && pos.count<PAWN>(~us) >= 1;
  }

  /// imbalance() calculates the imbalance by comparing the piece count of each
  /// piece type for both colors.
  template<Color Us>
  int imbalance(const int pieceCount[][PIECE_TYPE_NB]) {

    constexpr Color Them = (Us == WHITE ? BLACK : WHITE);

    int bonus = 0;

    // Third-degree polynomial material imbalance, by Tord Romstad and Stefan Geschwentner
    for (int pt1 = NO_PIECE_TYPE; pt1 <= QUEEN; ++pt1)
    {
        if (!pieceCount[Us][pt1])
            continue;

        int v = Linear[pt1];

        for (int pt2 = NO_PIECE_TYPE; pt2 <= pt1; ++pt2)
        {
            int w = 0;
            for (int pt3 = NO_PIECE_TYPE; pt3 <= pt2; ++pt3)
                w += CubicOurs[pt1][pt2][pt3] * pieceCount[Us][pt3];

            for (int pt3 = NO_PIECE_TYPE; pt3 <= QUEEN; ++pt3)
                w += CubicTheirs[pt1][pt2][pt3] * pieceCount[Them][pt3];

            v +=  QuadraticOurs[pt1][pt2] * pieceCount[Us][pt2]
                + QuadraticTheirs[pt1][pt2] * pieceCount[Them][pt2]
                + w * pieceCount[Us][pt2];
        }

        bonus += pieceCount[Us][pt1] * v;
    }

    return bonus;
  }

} // namespace

namespace Material {

/// Material::probe() looks up the current position's material configuration in
/// the material hash table. It returns a pointer to the Entry if the position
/// is found. Otherwise a new Entry is computed and stored there, so we don't
/// have to recompute all when the same material configuration occurs again.

Entry* probe(const Position& pos) {

  Key key = pos.material_key();
  Entry* e = pos.this_thread()->materialTable[key];

  if (e->key == key)
      return e;

  std::memset(e, 0, sizeof(Entry));
  e->key = key;
  e->factor[WHITE] = e->factor[BLACK] = (uint8_t)SCALE_FACTOR_NORMAL;

  Value npm_w = pos.non_pawn_material(WHITE);
  Value npm_b = pos.non_pawn_material(BLACK);
  Value npm = std::max(EndgameLimit, std::min(npm_w + npm_b, MidgameLimit));

  // Map total non-pawn material into [PHASE_ENDGAME, PHASE_MIDGAME]
  e->gamePhase = Phase(((npm - EndgameLimit) * PHASE_MIDGAME) / (MidgameLimit - EndgameLimit));

  // Let's look if we have a specialized evaluation function for this particular
  // material configuration. Firstly we look for a fixed configuration one, then
  // for a generic one if the previous search failed.
  if ((e->evaluationFunction = pos.this_thread()->endgames.probe<Value>(key)) != nullptr)
      return e;

  for (Color c = WHITE; c <= BLACK; ++c)
      if (is_KXK(pos, c))
      {
          e->evaluationFunction = &EvaluateKXK[c];
          return e;
      }

  // OK, we didn't find any special evaluation function for the current material
  // configuration. Is there a suitable specialized scaling function?
  EndgameBase<ScaleFactor>* sf;

  if ((sf = pos.this_thread()->endgames.probe<ScaleFactor>(key)) != nullptr)
  {
      e->scalingFunction[sf->strongSide] = sf; // Only strong color assigned
      return e;
  }

  // We didn't find any specialized scaling function, so fall back on generic
  // ones that refer to more than one material distribution. Note that in this
  // case we don't return after setting the function.
  for (Color c = WHITE; c <= BLACK; ++c)
  {
    if (is_KBPsK(pos, c))
        e->scalingFunction[c] = &ScaleKBPsK[c];

    else if (is_KQKRPs(pos, c))
        e->scalingFunction[c] = &ScaleKQKRPs[c];
  }

  if (npm_w + npm_b == VALUE_ZERO && pos.pieces(PAWN)) // Only pawns on the board
  {
      if (!pos.count<PAWN>(BLACK))
      {
          assert(pos.count<PAWN>(WHITE) >= 2);

          e->scalingFunction[WHITE] = &ScaleKPsK[WHITE];
      }
      else if (!pos.count<PAWN>(WHITE))
      {
          assert(pos.count<PAWN>(BLACK) >= 2);

          e->scalingFunction[BLACK] = &ScaleKPsK[BLACK];
      }
      else if (pos.count<PAWN>(WHITE) == 1 && pos.count<PAWN>(BLACK) == 1)
      {
          // This is a special case because we set scaling functions
          // for both colors instead of only one.
          e->scalingFunction[WHITE] = &ScaleKPKP[WHITE];
          e->scalingFunction[BLACK] = &ScaleKPKP[BLACK];
      }
  }

  // Zero or just one pawn makes it difficult to win, even with a small material
  // advantage. This catches some trivial draws like KK, KBK and KNK and gives a
  // drawish scale factor for cases such as KRKBP and KmmKm (except for KBBKN).
  if (!pos.count<PAWN>(WHITE) && npm_w - npm_b <= BishopValueMg)
      e->factor[WHITE] = uint8_t(npm_w <  RookValueMg   ? SCALE_FACTOR_DRAW :
                                 npm_b <= BishopValueMg ? 4 : 14);

  if (!pos.count<PAWN>(BLACK) && npm_b - npm_w <= BishopValueMg)
      e->factor[BLACK] = uint8_t(npm_b <  RookValueMg   ? SCALE_FACTOR_DRAW :
                                 npm_w <= BishopValueMg ? 4 : 14);

  // Evaluate the material imbalance. We use PIECE_TYPE_NONE as a place holder
  // for the bishop pair "extended piece", which allows us to be more flexible
  // in defining bishop pair bonuses.
  const int pieceCount[COLOR_NB][PIECE_TYPE_NB] = {
  { pos.count<BISHOP>(WHITE) > 1, pos.count<PAWN>(WHITE), pos.count<KNIGHT>(WHITE),
    pos.count<BISHOP>(WHITE)    , pos.count<ROOK>(WHITE), pos.count<QUEEN >(WHITE) },
  { pos.count<BISHOP>(BLACK) > 1, pos.count<PAWN>(BLACK), pos.count<KNIGHT>(BLACK),
    pos.count<BISHOP>(BLACK)    , pos.count<ROOK>(BLACK), pos.count<QUEEN >(BLACK) } };

  e->value = int16_t((imbalance<WHITE>(pieceCount) - imbalance<BLACK>(pieceCount)) / 16);
  return e;
}

} // namespace Material