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
#define _mCALL_3(FN, A, ...) FN(A), _mCALL_2(FN, __VA_ARGS__)
#define _mCALL_4(FN, A, ...) FN(A), _mCALL_3(FN, __VA_ARGS__)
#define _mCALL_5(FN, A, ...) FN(A), _mCALL_4(FN, __VA_ARGS__)
#define _mCALL_6(FN, A, ...) FN(A), _mCALL_5(FN, __VA_ARGS__)
#define _mCALL_7(FN, A, ...) FN(A), _mCALL_6(FN, __VA_ARGS__)
#define _mCALL_8(FN, A, ...) FN(A), _mCALL_7(FN, __VA_ARGS__)
#define _mCALL_9(FN, A, ...) FN(A), _mCALL_8(FN, __VA_ARGS__)

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
#define _mEVEN_2(A, B, ...) A, _mIDENT(_mEVEN_1(__VA_ARGS__))
#define _mEVEN_3(A, B, ...) A, _mIDENT(_mEVEN_2(__VA_ARGS__))
#define _mEVEN_4(A, B, ...) A, _mIDENT(_mEVEN_3(__VA_ARGS__))
#define _mEVEN_5(A, B, ...) A, _mIDENT(_mEVEN_4(__VA_ARGS__))
#define _mEVEN_6(A, B, ...) A, _mIDENT(_mEVEN_5(__VA_ARGS__))
#define _mEVEN_7(A, B, ...) A, _mIDENT(_mEVEN_6(__VA_ARGS__))
#define _mEVEN_8(A, B, ...) A, _mIDENT(_mEVEN_7(__VA_ARGS__))
#define _mEVEN_9(A, B, ...) A, _mIDENT(_mEVEN_7(__VA_ARGS__))

#define _mODD_0(...)
#define _mODD_1(A, B, ...) B
#define _mODD_2(A, B, ...) B, _mIDENT(_mODD_1(__VA_ARGS__))
#define _mODD_3(A, B, ...) B, _mIDENT(_mODD_2(__VA_ARGS__))
#define _mODD_4(A, B, ...) B, _mIDENT(_mODD_3(__VA_ARGS__))
#define _mODD_5(A, B, ...) B, _mIDENT(_mODD_4(__VA_ARGS__))
#define _mODD_6(A, B, ...) B, _mIDENT(_mODD_5(__VA_ARGS__))
#define _mODD_7(A, B, ...) B, _mIDENT(_mODD_6(__VA_ARGS__))
#define _mODD_8(A, B, ...) B, _mIDENT(_mODD_7(__VA_ARGS__))
#define _mODD_9(A, B, ...) B, _mIDENT(_mODD_7(__VA_ARGS__))

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
