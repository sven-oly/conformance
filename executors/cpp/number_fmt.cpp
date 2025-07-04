/********************************************************************
 * testing icu4c number format
 */

#include <json-c/json.h>

#include <unicode/bytestream.h>
#include <unicode/compactdecimalformat.h>
#include <unicode/currunit.h>
#include <unicode/dcfmtsym.h>
#include <unicode/fieldpos.h>
#include <unicode/fpositer.h>
#include <unicode/locid.h>
#include <unicode/measunit.h>
#include <unicode/nounit.h>
#include <unicode/numberformatter.h>
#include <unicode/numberrangeformatter.h>
#include <unicode/numfmt.h>
#include <unicode/numsys.h>
#include <unicode/stringpiece.h>
#include <unicode/uclean.h>
#include <unicode/ucurr.h>
#include <unicode/unistr.h>
#include <unicode/unum.h>
#include <unicode/unumberformatter.h>
#include <unicode/uobject.h>
#include <unicode/utypes.h>

#include <cstdio>
#include <cstdlib>

#include <iostream>
#include <regex>
#include <string>
#include <cstring>

#include "./util.h"

using std::cout;
using std::endl;
using std::string;

using icu::CurrencyUnit;
using icu::Locale;
using icu::MeasureUnit;
using icu::NumberingSystem;
using icu::UnicodeString;

using icu::number::FormattedNumber;
using icu::number::impl::UFormattedNumberData;
using icu::number::IntegerWidth;
using icu::number::LocalizedNumberFormatter;
using icu::number::NumberFormatter;
using icu::number::NumberFormatterSettings;
using icu::number::Notation;
using icu::number::Precision;
using icu::number::Scale;

// Get the integer value of a settting
auto get_integer_setting(string key_value_string) -> int16_t {
  int16_t return_val = -1;
  size_t colon_pos = key_value_string.find(":");
  if (colon_pos >= 0) {
    string num_string = key_value_string.substr(colon_pos +1);
    return_val = std::stoi(num_string);
  }
  return return_val;
}

// Get the double value of a settting
auto get_double_setting(string key_value_string) -> double {
  double return_val = -1.0;
  size_t colon_pos = key_value_string.find(":");
  if (colon_pos >= 0) {
    string num_string = key_value_string.substr(colon_pos +1);
    return_val = std::stod(num_string);
  }
  return return_val;
}

// Get integer width settings
auto set_integerWidth(json_object* options_obj) -> IntegerWidth {
  IntegerWidth integerWidth_setting = IntegerWidth::zeroFillTo(1);
  if (options_obj == nullptr) {
    return integerWidth_setting;
  }
  string int_width_string;

  json_object* integer_obj_min = json_object_object_get(
      options_obj, "minimumIntegerDigits");
  json_object* integer_obj_max = json_object_object_get(
      options_obj, "maximumIntegerDigits");
  if ((integer_obj_min != nullptr) && (integer_obj_max != nullptr)) {
    int_width_string = json_object_get_string(integer_obj_min);
    int32_t val_min32 = get_integer_setting(int_width_string);
    int_width_string = json_object_get_string(integer_obj_max);
    int32_t val_max32 = get_integer_setting(int_width_string);
    integerWidth_setting =
        IntegerWidth::zeroFillTo(val_min32).truncateAt(val_max32);
  } else if ((integer_obj_min != nullptr) && (integer_obj_max == nullptr)) {
    int_width_string = json_object_get_string(integer_obj_min);
    int32_t val_min32 = get_integer_setting(int_width_string);
    int_width_string = json_object_get_string(integer_obj_min);
    integerWidth_setting = IntegerWidth::zeroFillTo(val_min32);
  } else if ((integer_obj_min == nullptr) && (integer_obj_max != nullptr)) {
    int_width_string = json_object_get_string(integer_obj_max);
    int32_t val_max32 = get_integer_setting(int_width_string);
    integerWidth_setting = IntegerWidth::zeroFillTo(1).truncateAt(val_max32);
  }
  return integerWidth_setting;
}

// Get fraction and siginfication digits settings
auto set_precision_digits(json_object* options_obj,
                               Precision previous_setting) -> Precision {
  Precision precision_setting = previous_setting;
  if (options_obj == nullptr) {
    return precision_setting;
  }

  // First, consider fraction digits.
  json_object* precision_obj_max =
      json_object_object_get(options_obj, "maximumFractionDigits");
  json_object* precision_obj_min =
      json_object_object_get(options_obj, "minimumFractionDigits");

  string precision_string;

  int16_t val_max = 0;
  int16_t val_min = 0;
  if (precision_obj_max != nullptr) {
    val_max = get_integer_setting(
        json_object_get_string(precision_obj_max));
  }
  if (precision_obj_min != nullptr) {
    val_min = get_integer_setting(
        json_object_get_string(precision_obj_min));
  }
  if ((precision_obj_max != nullptr) && (precision_obj_min != nullptr)) {
    // Both are set
    precision_setting = Precision::minMaxFraction(val_min, val_max);
  } else if ((precision_obj_max == nullptr) && (precision_obj_min != nullptr)) {
    precision_setting = Precision::minFraction(val_min);
  } else if ((precision_obj_max != nullptr) && (precision_obj_min == nullptr)) {
    precision_setting = Precision::maxFraction(val_max);
  }

  // Now handle significant digits
  precision_obj_max =
      json_object_object_get(options_obj, "maximumSignificantDigits");
  precision_obj_min =
      json_object_object_get(options_obj, "minimumSignificantDigits");

  if (precision_obj_max != nullptr) {
    val_max = get_integer_setting(
        json_object_get_string(precision_obj_max));
  }
  if (precision_obj_min != nullptr) {
    val_min = get_integer_setting(
        json_object_get_string(precision_obj_min));
  }

  if ((precision_obj_max != nullptr) && (precision_obj_min != nullptr)) {
    // Both are set
    precision_setting = Precision::minMaxSignificantDigits(val_min, val_max);
  } else if ((precision_obj_max == nullptr) && (precision_obj_min != nullptr)) {
    precision_setting = Precision::minSignificantDigits(val_min);
  } else if ((precision_obj_max != nullptr) && (precision_obj_min == nullptr)) {
    precision_setting = Precision::maxSignificantDigits(val_max);
  }

  return precision_setting;
}

auto set_sign_display(json_object* options_obj) -> UNumberSignDisplay {
  UNumberSignDisplay signDisplay_setting = UNUM_SIGN_AUTO;

  if (options_obj == nullptr) {
    return signDisplay_setting;
  }

  json_object* signDisplay_obj =
      json_object_object_get(options_obj, "signDisplay");
  if (signDisplay_obj != nullptr) {
    string signDisplay_string = json_object_get_string(signDisplay_obj);

    if (signDisplay_string == "exceptZero") {
      signDisplay_setting = UNUM_SIGN_EXCEPT_ZERO;
    } else if (signDisplay_string == "always") {
      signDisplay_setting = UNUM_SIGN_ALWAYS;
    } else if (signDisplay_string == "never") {
      signDisplay_setting = UNUM_SIGN_NEVER;
    } else if (signDisplay_string == "negative") {
      signDisplay_setting = UNUM_SIGN_NEGATIVE;
    }else if (signDisplay_string == "accounting") {
      signDisplay_setting = UNUM_SIGN_ACCOUNTING;
    } else if (signDisplay_string == "accounting_exceptZero") {
      signDisplay_setting = UNUM_SIGN_ACCOUNTING_EXCEPT_ZERO;
    }else if (signDisplay_string == "accounting_negative") {
      signDisplay_setting = UNUM_SIGN_ACCOUNTING_NEGATIVE;
    }
  }
  return signDisplay_setting;
}

auto TestNumfmt(json_object *json_in) -> string {
  UErrorCode status = U_ZERO_ERROR;

  // label information
  json_object *label_obj = json_object_object_get(json_in, "label");
  string label_string = json_object_get_string(label_obj);

  // Other parts of the input.
  json_object *options_obj = json_object_object_get(json_in, "options");
  json_object *skeleton_obj = json_object_object_get(json_in, "skeleton");

  // The locale for numbers
  json_object *locale_label_obj = json_object_object_get(json_in, "locale");
  string locale_string = "und";
  if (locale_label_obj != nullptr) {
    locale_string = json_object_get_string(locale_label_obj);
  }

  // JSON for the results
  json_object *return_json = json_object_new_object();
  json_object_object_add(return_json, "label", label_obj);

  json_object *pattern_obj = json_object_object_get(json_in, "pattern");
  string pattern = "";
  if (pattern_obj) {
    pattern = json_object_get_string(pattern_obj);

    // Check for unsupported patterns.
    std::regex check1("(0+0\\.#+E)");
    std::regex check2("(^\\.0#*E)");
    std::smatch m;

    if (std::regex_search(pattern, m, check1) || std::regex_search(pattern, m, check2)) {
      // No, not a supported pattern
      // Report a failure
      json_object_object_add(
          return_json,
          "error_type",
          json_object_new_string("unsupported"));
      json_object_object_add(
          return_json,
          "unsupported",
          json_object_new_string("unsupported pattern"));
      json_object_object_add(
          return_json,
          "error_detail",
          json_object_new_string(pattern.c_str()));

      // Nothing more to do here.
      string return_string = json_object_to_json_string(return_json);
      return return_string;
    }
  }

  const Locale displayLocale(locale_string.c_str());

  // Get options
  json_object *notation_obj;
  json_object *unit_obj;
  json_object *unitDisplay_obj;
  json_object *style_obj;
  json_object *currencyDisplay_obj;
  json_object *group_obj;
  json_object *precision_obj_min;
  json_object *precision_obj_max;
  json_object *min_integer_digits_obj;
  json_object *max_integer_digits_obj;
  json_object *roundingMode_obj;
  json_object *compactDisplay_obj;
  json_object *currency_obj;
  json_object *signDisplay_obj;

  string notation_string;
  string precision_string;
  string unit_string;
  string unitDisplay_string;
  string style_string;
  string compactDisplay_string;
  string roundingMode_string;

  // Defaults for settings.
  CurrencyUnit currency_unit_setting = CurrencyUnit();
  IntegerWidth integerWidth_setting = IntegerWidth::zeroFillTo(1);
  MeasureUnit unit_setting = icu::NoUnit::base();
  Notation notation_setting = Notation::simple();

  // TODO?  = Precision::unlimited();
  Precision precision_setting = Precision::integer();
  Scale scale_setting = Scale::none();
  UNumberSignDisplay signDisplay_setting = UNUM_SIGN_AUTO;
  UNumberFormatRoundingMode rounding_setting = UNUM_ROUND_HALFEVEN;
  UNumberDecimalSeparatorDisplay separator_setting =
      UNUM_DECIMAL_SEPARATOR_AUTO;
  UNumberGroupingStrategy grouping_setting = UNUM_GROUPING_AUTO;
  UNumberUnitWidth unit_width_setting =
      UNumberUnitWidth::UNUM_UNIT_WIDTH_NARROW;

  NumberingSystem* numbering_system =
      NumberingSystem::createInstance(displayLocale, status);

  // Check all the options
  if (options_obj != nullptr) {
    signDisplay_setting = set_sign_display(options_obj);

    notation_obj = json_object_object_get(options_obj, "notation");
    if (notation_obj != nullptr) {
      notation_string = json_object_get_string(notation_obj);
    }

    // TODO: Initialize setting based on this string.
    unit_obj = json_object_object_get(options_obj, "unit");
    if (unit_obj != nullptr) {
      unit_string = json_object_get_string(unit_obj);
      if (unit_string == "percent") {
        unit_setting = icu::NoUnit::percent();
      } else if (unit_string == "permille") {
        unit_setting = icu::NoUnit::permille();
      } else if (unit_string == "furlong") {
        unit_setting = MeasureUnit::getFurlong();
      }
    }

    unitDisplay_obj = json_object_object_get(options_obj, "unitDisplay");
    if (unitDisplay_obj != nullptr) {
      unitDisplay_string = json_object_get_string(unitDisplay_obj);
      if (unitDisplay_string == "narrow") {
        unit_width_setting = UNumberUnitWidth::UNUM_UNIT_WIDTH_NARROW;
      } else if (unitDisplay_string == "long") {
        unit_width_setting = UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME;
      }
    }

    style_obj = json_object_object_get(options_obj, "style");
    if (style_obj != nullptr) {
      style_string = json_object_get_string(style_obj);
    }

    compactDisplay_obj = json_object_object_get(options_obj, "compactDisplay");
    if (compactDisplay_obj != nullptr) {
      compactDisplay_string = json_object_get_string(compactDisplay_obj);
      if (compactDisplay_string == "short") {
        notation_setting = Notation::compactShort();
      } else {
        notation_setting = Notation::compactLong();
      }
    }

    currency_obj = json_object_object_get(options_obj, "currency");
    if (currency_obj != nullptr) {
      string currency_string = json_object_get_string(currency_obj);
      // Set the unit to a currency value
      unit_setting = CurrencyUnit(icu::StringPiece(currency_string), status);
    }

    // TODO: make a function
    currencyDisplay_obj =
        json_object_object_get(options_obj, "currencyDisplay");
    if (currencyDisplay_obj != nullptr) {
      string currencyDisplay_string =
          json_object_get_string(currencyDisplay_obj);
      unit_width_setting = UNumberUnitWidth::UNUM_UNIT_WIDTH_NARROW;
      if (currencyDisplay_string == "narrowSymbol") {
        unit_width_setting = UNumberUnitWidth::UNUM_UNIT_WIDTH_NARROW;
      } else if (currencyDisplay_string == "symbol") {
        unit_width_setting = UNumberUnitWidth::UNUM_UNIT_WIDTH_SHORT;
      } else if (currencyDisplay_string == "name") {
        unit_width_setting = UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME;
      }
    }

    // TODO: Make this a function rather than inline.
    roundingMode_obj = json_object_object_get(options_obj, "roundingMode");
    if (roundingMode_obj != nullptr) {
      roundingMode_string = json_object_get_string(roundingMode_obj);
      if (roundingMode_string == "floor") {
        rounding_setting = UNUM_ROUND_FLOOR;
      } else if (roundingMode_string == "ceil") {
        rounding_setting = UNUM_ROUND_CEILING;
      } else if (roundingMode_string == "halfEven") {
        rounding_setting = UNUM_ROUND_HALFEVEN;
      } else if (roundingMode_string == "halfOdd") {
        rounding_setting = UNUM_ROUND_HALF_ODD;
      } else if (roundingMode_string == "halfTrunc") {
        rounding_setting = UNUM_ROUND_HALFDOWN;
      } else if (roundingMode_string == "halfExpand") {
        rounding_setting = UNUM_ROUND_HALFUP;
      } else if (roundingMode_string == "trunc") {
        rounding_setting = UNUM_ROUND_DOWN;
      } else if (roundingMode_string == "expand") {
        rounding_setting = UNUM_ROUND_UP;
      } else if (roundingMode_string == "unnecessary") {
        rounding_setting = UNUM_ROUND_UNNECESSARY;
      }
    }

    // TODO: make a function for group_setting
    group_obj = json_object_object_get(options_obj, "useGrouping");
    if (group_obj != nullptr) {
      string group_string = json_object_get_string(group_obj);
      if (group_string == "false" || group_string == "off") {
        grouping_setting = UNUM_GROUPING_OFF;
      } else if (group_string == "true" || group_string == "auto") {
        grouping_setting = UNUM_GROUPING_AUTO;
      } else if (group_string == "on_aligned") {
        grouping_setting = UNUM_GROUPING_ON_ALIGNED;
      } else if (group_string == "mim2") {
        grouping_setting = UNUM_GROUPING_MIN2;
      } else if (group_string == "thousands") {
        grouping_setting = UNUM_GROUPING_THOUSANDS;
      } else if (group_string == "count") {
        grouping_setting = UNUM_GROUPING_COUNT;
      }
    }

    // Need to avoid resetting when not options are specifierd.
    precision_setting = set_precision_digits(options_obj, precision_setting);

    // Minimum integer digits
    integerWidth_setting = set_integerWidth(options_obj);

    // Check on scaling the value
    json_object* scale_obj = json_object_object_get(
        options_obj, "conformanceScale");
    if (scale_obj != nullptr) {
      string scale_string = json_object_get_string(scale_obj);
      double scale_val = get_double_setting(scale_string);
      scale_setting = Scale::byDouble(scale_val);
    }

    // Other settings...
    // NumberFormatter::with().symbols(
    //     DecimalFormatSymbols(Locale("de_CH"), status))

    json_object* numbering_system_obj =
        json_object_object_get(options_obj, "numberingSystem");
    if (numbering_system_obj != nullptr) {
      string numbering_system_string =
          json_object_get_string(numbering_system_obj);
      numbering_system = NumberingSystem::createInstanceByName(
          numbering_system_string.c_str(), status);
    }

    // Handling decimal point
    json_object* decimal_always_obj =
        json_object_object_get(options_obj, "conformanceDecimalAlways");
    if (decimal_always_obj != nullptr) {
      string separator_string = json_object_get_string(
          decimal_always_obj);
      if (separator_string == "true") {
        separator_setting = UNUM_DECIMAL_SEPARATOR_ALWAYS;
      }
    }
  }


  // Additional parameters and values
  json_object *input_obj = json_object_object_get(json_in, "input");
  string input_string = json_object_get_string(input_obj);

  // Start using these things

  int32_t chars_out;  // Results of extracting characters from Unicode string
  bool no_error = true;

  LocalizedNumberFormatter nf;
  if (notation_string == "scientific") {
    notation_setting = Notation::scientific();
    if (options_obj != nullptr) {
      json_object* conformanceExponent_obj =
          json_object_object_get(options_obj, "conformanceExponent");
      if (conformanceExponent_obj != nullptr) {
        // Check for the number of digits and sign
        string confExp_string = json_object_get_string(conformanceExponent_obj);
        // https://unicode-org.github.io/icu-docs/apidoc/released/icu4c/unumberformatter_8h.html#a18092ae1533c9c260f01c9dbf25589c9
        // TODO: Parse to find the number of values and the sign setting
        if (confExp_string == "+ee") {
          notation_setting = Notation::scientific().withMinExponentDigits(2)
                             .withExponentSignDisplay(UNUM_SIGN_ALWAYS);
        }
      }
    }
  }

  if (skeleton_obj != nullptr) {
    // Use the skeleton provided
    UnicodeString unicode_skeleton_string;
    string skeleton_string = json_object_get_string(skeleton_obj);
    unicode_skeleton_string = skeleton_string.c_str();

    nf = NumberFormatter::forSkeleton(
        unicode_skeleton_string, status).locale(displayLocale);
  } else {
    // Use settings to initialize the formatter
    nf = NumberFormatter::withLocale(displayLocale)
         .notation(notation_setting)
         .decimal(separator_setting)
         .precision(precision_setting)
         .integerWidth(integerWidth_setting)
         .grouping(grouping_setting)
         .adoptSymbols(numbering_system)
         .roundingMode(rounding_setting)
         .scale(scale_setting)
         .sign(signDisplay_setting)
         .unit(unit_setting)
         .unitWidth(unit_width_setting);
  }

  if (U_FAILURE(status) != 0) {
    const char* error_name = u_errorName(status);
    json_object_object_add(
        return_json,
        "error", json_object_new_string("error in constructor"));
    json_object_object_add(
        return_json,
        "error_detail", json_object_new_string(error_name));
    no_error = false;
  }

  if (no_error) {
    UnicodeString number_result;
    // Use formatDecimal, passing the string instead of a double.
    FormattedNumber fmt_number = nf.formatDecimal(input_string, status);
    number_result = fmt_number.toString(status);
    if (U_FAILURE(status) != 0) {
      const char* error_name = u_errorName(status);
      json_object_object_add(
          return_json,
          "error", json_object_new_string("error in toString"));
      json_object_object_add(
          return_json,
          "error_detail", json_object_new_string(error_name));
    }

    // Get the resulting value as a string
    string test_result;
    number_result.toUTF8String(test_result);

    if (U_FAILURE(status) != 0) {
      // Report a failure
      if (status == U_FORMAT_INEXACT_ERROR) {
        const char* error_name = u_errorName(status);
        // Inexact result is unsupported.
        json_object_object_add(
            return_json,
            "error_type",
            json_object_new_string("unsupported"));
        json_object_object_add(
            return_json,
            "unsupported",
            json_object_new_string(error_name));
      }
      else {
        const char* error_name = u_errorName(status);

        json_object_object_add(
            return_json, "error",
            json_object_new_string("error getting result string"));
        json_object_object_add(
            return_json, "error_detail", json_object_new_string(error_name));
      }
    } else {
      // It worked!
      json_object_object_add(return_json,
                             "result",
                             json_object_new_string(test_result.c_str()));
    }
  }

  // To see what was actually used.
  UnicodeString u_skeleton_out = nf.toSkeleton(status);

  string skeleton_string;
  u_skeleton_out.toUTF8String(skeleton_string);

  json_object_object_add(return_json,
                         "actual_skeleton",
                         json_object_new_string(skeleton_string.c_str()));

  string return_string = json_object_to_json_string(return_json);
  return return_string;
}
