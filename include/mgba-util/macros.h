/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_MACROS_H
#define M_MACROS_H

#define _mCPP_CAT(A, B) A ## B

#define _mIDENT(...) __VA_ARGS__
#define _mCALL(FN, ...) _mIDENT(FN(__VA_ARGS__))
#define _mCAT(A, B) _mCPP_CAT(A, B)
#define _mSTRINGIFY(X, ...) #X

#define _mCALL_0(FN, ...)
#define _mCALL_1(FN, A) FN(A)
#define _mCALL_2(FN, A, B) FN(A), FN(B)
#define _mCALL_3(FN, A, B, C) FN(A), FN(B), FN(C)
#define _mCALL_4(FN, A, B, C, D) FN(A), FN(B), FN(C), FN(D)
#define _mCALL_5(FN, A, B, C, D, E) FN(A), FN(B), FN(C), FN(D), FN(E)
#define _mCALL_6(FN, A, B, C, D, E, F) FN(A), FN(B), FN(C), FN(D), FN(E), FN(F)
#define _mCALL_7(FN, A, B, C, D, E, F, G) FN(A), FN(B), FN(C), FN(D), FN(E), FN(F), FN(G)
#define _mCALL_8(FN, A, B, C, D, E, F, G, H) FN(A), FN(B), FN(C), FN(D), FN(E), FN(F), FN(G), FN(H)
#define _mCALL_9(FN, A, B, C, D, E, F, G, H, I) FN(A), FN(B), FN(C), FN(D), FN(E), FN(F), FN(G), FN(H), FN(I)

#define _mCOMMA_0(N, ...) N
#define _mCOMMA_1(N, ...) N, __VA_ARGS__
#define _mCOMMA_2(N, ...) N, __VA_ARGS__
#define _mCOMMA_3(N, ...) N, __VA_ARGS__
#define _mCOMMA_4(N, ...) N, __VA_ARGS__
#define _mCOMMA_5(N, ...) N, __VA_ARGS__
#define _mCOMMA_6(N, ...) N, __VA_ARGS__
#define _mCOMMA_7(N, ...) N, __VA_ARGS__
#define _mCOMMA_8(N, ...) N, __VA_ARGS__
#define _mCOMMA_9(N, ...) N, __VA_ARGS__

#define _mEVEN_0(...)
#define _mEVEN_1(A, B, ...) A
#define _mEVEN_2(A, B, C, D, ...) A, C
#define _mEVEN_3(A, B, C, D, E, F, ...) A, C, E
#define _mEVEN_4(A, B, C, D, E, F, G, H, ...) A, C, E, G
#define _mEVEN_5(A, B, C, D, E, F, G, H, I, J, ...) A, C, E, G, I
#define _mEVEN_6(A, B, ...) A, _mIDENT(_mEVEN_5(__VA_ARGS__))
#define _mEVEN_7(A, B, C, D, ...) A, C, _mIDENT(_mEVEN_5(__VA_ARGS__))
#define _mEVEN_8(A, B, C, D, E, F, ...) A, C, E, _mIDENT(_mEVEN_5(__VA_ARGS__))
#define _mEVEN_9(A, B, C, D, E, F, G, H, ...) A, C, E, G, _mIDENT(_mEVEN_5(__VA_ARGS__))

#define _mODD_0(...)
#define _mODD_1(A, B, ...) B
#define _mODD_2(A, B, C, D, ...) B, D
#define _mODD_3(A, B, C, D, E, F, ...) B, D, F
#define _mODD_4(A, B, C, D, E, F, G, H, ...) B, D, F, H
#define _mODD_5(A, B, C, D, E, F, G, H, I, J, ...) B, D, F, H, J
#define _mODD_6(A, B, ...) B, _mIDENT(_mODD_5(__VA_ARGS__))
#define _mODD_7(A, B, C, D, ...) B, D, _mIDENT(_mODD_5(__VA_ARGS__))
#define _mODD_8(A, B, C, D, E, F, ...) B, D, F, _mIDENT(_mODD_5(__VA_ARGS__))
#define _mODD_9(A, B, C, D, E, F, G, H, ...) B, D, F, H, _mIDENT(_mODD_5(__VA_ARGS__))

#define _mIF0_0(...) __VA_ARGS__
#define _mIF0_1(...)
#define _mIF0_2(...)
#define _mIF0_3(...)
#define _mIF0_4(...)
#define _mIF0_5(...)
#define _mIF0_6(...)
#define _mIF0_7(...)
#define _mIF0_8(...)
#define _mIF0_9(...)

#define _mSUCC_0 1
#define _mSUCC_1 2
#define _mSUCC_2 3
#define _mSUCC_3 4
#define _mSUCC_4 5
#define _mSUCC_5 6
#define _mSUCC_6 7
#define _mSUCC_7 8
#define _mSUCC_8 9

#define _mPRED_1 0
#define _mPRED_2 1
#define _mPRED_3 2
#define _mPRED_4 3
#define _mPRED_5 4
#define _mPRED_6 5
#define _mPRED_7 6
#define _mPRED_8 7
#define _mPRED_9 8

#endif
