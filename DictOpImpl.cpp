#define _FXASSERT_H_ // prohibit logging system from kicking in

#include "stdafx.h"
#include "DictOpImpl.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			DictOpImpl.cpp
// Author		intelfx
// Description	Dictionary normalisations implementation (assignment #4.a).
// -----------------------------------------------------------------------------

// WARNING Non-implemented since KDevelop 4 fails to parse xpressive header file.
// TODO Wait for KDevelop 4 to fix their parser, or rewrite for boost::regex.

// #include <boost/xpressive/xpressive.hpp>
// using namespace boost::xpressive;

bool normalisation_apostrophes (std::wstring& str)
{
// 	static wsregex apostrophe_match = (as_xpr (L'\'') >> (s1= !(~_n)) >> eos);
//
// 	if (!regex_search (str, apostrophe_match))
// 		return 0;
//
// 	str = boost::xpressive::regex_replace (str, apostrophe_match, L"$1");
// 	return 1;

	return 0;
}

bool normalisation_plural (std::wstring& str)
{
// 	static wsregex plural_match = (as_xpr (L"s") | as_xpr (L"es")) >> eos;
//
// 	if (!regex_search (str, plural_match))
// 		return 0;
//
// 	str = boost::xpressive::regex_replace (str, plural_match, L"");
// 	return 1;

	return 0;
}

bool normalisation_participle (std::wstring& str)
{
// 	static wsregex participle_match = (as_xpr (L"ed") | as_xpr (L"t")) >> eos;
//
// 	if (!regex_search (str, participle_match))
// 		return 0;
//
// 	str = boost::xpressive::regex_replace (str, participle_match, L"");
// 	return 1;

	return 0;
}

bool normalisation_prparticiple (std::wstring& str)
{
// 	static wsregex pr_participle_match = as_xpr (L"ing") >> eos;
//
// 	if (!regex_search (str, pr_participle_match))
// 		return 0;
//
// 	str = boost::xpressive::regex_replace (str, pr_participle_match, L"");
// 	return 1;

	return 0;
}