/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

struct Configuration;
struct mCore;

namespace QGBA {

class Override {
public:
	virtual ~Override() {}

	virtual void apply(struct mCore*) = 0;
	virtual void identify(const struct mCore*) = 0;
	virtual void save(struct Configuration*) const = 0;
};

}
