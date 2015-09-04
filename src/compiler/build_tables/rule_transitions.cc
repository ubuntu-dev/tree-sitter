#include "compiler/build_tables/rule_transitions.h"
#include "compiler/build_tables/rule_can_be_blank.h"
#include "compiler/build_tables/merge_transitions.h"
#include "compiler/rules/blank.h"
#include "compiler/rules/choice.h"
#include "compiler/rules/seq.h"
#include "compiler/rules/repeat.h"
#include "compiler/rules/metadata.h"
#include "compiler/rules/symbol.h"
#include "compiler/rules/character_set.h"
#include "compiler/rules/visitor.h"

namespace tree_sitter {
namespace build_tables {

using std::map;
using std::make_shared;
using rules::CharacterSet;
using rules::Choice;
using rules::Symbol;

template <typename T>
void merge_transitions(map<T, rule_ptr> *, const map<T, rule_ptr> &);

struct MergeAsChoice {
  void operator()(rule_ptr *left, const rule_ptr *right) {
    *left = Choice::build({ *left, *right });
  }
};

template <>
void merge_transitions(map<CharacterSet, rule_ptr> *left,
                       const map<CharacterSet, rule_ptr> &right) {
  for (auto &pair : right)
    merge_char_transition<rule_ptr>(left, pair, MergeAsChoice());
}

template <>
void merge_transitions(map<Symbol, rule_ptr> *left,
                       const map<Symbol, rule_ptr> &right) {
  for (auto &pair : right)
    merge_sym_transition<rule_ptr>(left, pair, MergeAsChoice());
}

template <typename T>
class RuleTransitions : public rules::RuleFn<map<T, rule_ptr>> {
 private:
  map<T, rule_ptr> apply_to_primitive(const Rule *rule) {
    auto primitive = dynamic_cast<const T *>(rule);
    if (primitive)
      return map<T, rule_ptr>({ { *primitive, make_shared<rules::Blank>() } });
    else
      return map<T, rule_ptr>();
  }

  map<T, rule_ptr> apply_to(const CharacterSet *rule) {
    return apply_to_primitive(rule);
  }

  map<T, rule_ptr> apply_to(const Symbol *rule) {
    return apply_to_primitive(rule);
  }

  map<T, rule_ptr> apply_to(const rules::Choice *rule) {
    map<T, rule_ptr> result;
    for (const auto &el : rule->elements)
      merge_transitions<T>(&result, this->apply(el));
    return result;
  }

  map<T, rule_ptr> apply_to(const rules::Seq *rule) {
    auto result = this->apply(rule->left);
    for (auto &pair : result)
      pair.second = rules::Seq::build({ pair.second, rule->right });
    if (rule_can_be_blank(rule->left))
      merge_transitions<T>(&result, this->apply(rule->right));
    return result;
  }

  map<T, rule_ptr> apply_to(const rules::Repeat *rule) {
    auto result = this->apply(rule->content);
    for (auto &pair : result)
      pair.second = rules::Seq::build({ pair.second, rule->copy() });
    return result;
  }

  map<T, rule_ptr> apply_to(const rules::Metadata *rule) {
    auto result = this->apply(rule->rule);
    for (auto &pair : result)
      pair.second = make_shared<rules::Metadata>(pair.second, rule->value);
    return result;
  }
};

map<CharacterSet, rule_ptr> char_transitions(const rule_ptr &rule) {
  return RuleTransitions<CharacterSet>().apply(rule);
}

map<Symbol, rule_ptr> sym_transitions(const rule_ptr &rule) {
  return RuleTransitions<Symbol>().apply(rule);
}

}  // namespace build_tables
}  // namespace tree_sitter
