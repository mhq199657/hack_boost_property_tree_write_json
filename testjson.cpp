#define BOOST_PROPERTY_TREE_DETAIL_JSON_PARSER_STANDARD_CALLBACKS_HPP
#include <boost/assert.hpp>
#include <boost/property_tree/ptree.hpp>
#include <vector>

namespace boost
{
  namespace property_tree
  {
    namespace json_parser
    {
      namespace detail
      {

        namespace constants
        {
          template <typename Ch>
          const Ch *null_value();
          template <>
          inline const char *null_value() { return "null"; }
          template <>
          inline const wchar_t *null_value() { return L"null"; }

          template <typename Ch>
          const Ch *true_value();
          template <>
          inline const char *true_value() { return "true"; }
          template <>
          inline const wchar_t *true_value() { return L"true"; }

          template <typename Ch>
          const Ch *false_value();
          template <>
          inline const char *false_value() { return "false"; }
          template <>
          inline const wchar_t *false_value() { return L"false"; }
        } // namespace constants

        template <typename Ptree>
        class standard_callbacks
        {
        public:
          typedef typename Ptree::data_type string;
          typedef typename string::value_type char_type;

          void on_null()
          {
            new_value() = constants::null_value<char_type>();
          }

          void on_boolean(bool b)
          {
            new_value() = b ? constants::true_value<char_type>()
                            : constants::false_value<char_type>();
          }

          template <typename Range>
          void on_number(Range code_units)
          {
            new_value().assign(code_units.begin(), code_units.end());
          }
          void on_begin_number()
          {
            new_value();
          }
          void on_digit(char_type d)
          {
            current_value() += d;
          }
          void on_end_number() {}

          void on_begin_string()
          {
            new_value();
            current_value() += '"';
          }
          template <typename Range>
          void on_code_units(Range code_units)
          {
            current_value().append(code_units.begin(), code_units.end());
          }
          void on_code_unit(char_type c)
          {
            current_value() += c;
          }
          void on_end_string()
          {
            current_value() += '"';
          }

          void on_begin_array()
          {
            new_tree();
            stack.back().k = array;
          }
          void on_end_array()
          {
            if (stack.back().k == leaf)
              stack.pop_back();
            stack.pop_back();
          }

          void on_begin_object()
          {
            new_tree();
            stack.back().k = object;
          }
          void on_end_object()
          {
            if (stack.back().k == leaf)
              stack.pop_back();
            stack.pop_back();
          }

          Ptree &output() { return root; }

        protected:
          bool is_key() const
          {
            return stack.back().k == key;
          }
          string &current_value()
          {
            layer &l = stack.back();
            switch (l.k)
            {
            case key:
              return key_buffer;
            default:
              return l.t->data();
            }
          }

        private:
          Ptree root;
          string key_buffer;
          enum kind
          {
            array,
            object,
            key,
            leaf
          };
          struct layer
          {
            kind k;
            Ptree *t;
          };
          std::vector<layer> stack;

          Ptree &new_tree()
          {
            if (stack.empty())
            {
              layer l = {leaf, &root};
              stack.push_back(l);
              return root;
            }
            layer &l = stack.back();
            switch (l.k)
            {
            case array:
            {
              l.t->push_back(std::make_pair(string(), Ptree()));
              layer nl = {leaf, &l.t->back().second};
              stack.push_back(nl);
              return *stack.back().t;
            }
            case object:
            default:
              BOOST_ASSERT(false); // must start with string, i.e. call new_value
            case key:
            {
              l.t->push_back(std::make_pair(key_buffer, Ptree()));
              l.k = object;
              layer nl = {leaf, &l.t->back().second};
              stack.push_back(nl);
              return *stack.back().t;
            }
            case leaf:
              stack.pop_back();
              return new_tree();
            }
          }
          string &new_value()
          {
            if (stack.empty())
              return new_tree().data();
            layer &l = stack.back();
            switch (l.k)
            {
            case leaf:
              stack.pop_back();
              return new_value();
            case object:
              l.k = key;
              key_buffer.clear();
              return key_buffer;
            default:
              return new_tree().data();
            }
          }
        };

      } // namespace detail
    }   // namespace json_parser
  }     // namespace property_tree
} // namespace boost

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/optional.hpp>
#include <iostream>
#include <sstream>
#include <cstdlib>

namespace details
{
  template <class Ptree>
  bool verify_json(const Ptree &pt, int depth)
  {

    typedef typename Ptree::key_type::value_type Ch;
    typedef typename std::basic_string<Ch> Str;

    // Root ptree cannot have data
    if (depth == 0 && !pt.template get_value<Str>().empty())
      return false;

    // Ptree cannot have both children and data
    if (!pt.template get_value<Str>().empty() && !pt.empty())
      return false;

    // Check children
    typename Ptree::const_iterator it = pt.begin();
    for (; it != pt.end(); ++it)
      if (!verify_json(it->second, depth + 1))
        return false;

    // Success
    return true;
  }

  template <class Ch>
  std::basic_string<Ch> create_escapes(const std::basic_string<Ch> &s)
  {
    std::basic_string<Ch> result;
    typename std::basic_string<Ch>::const_iterator b = s.begin();
    typename std::basic_string<Ch>::const_iterator e = s.end();
    auto beg = b;
    while (b != e)
    {
      typedef typename boost::make_unsigned<Ch>::type UCh;
      UCh c(*b);
      // This assumes an ASCII superset. But so does everything in PTree.
      // We escape everything outside ASCII, because this code can't
      // handle high unicode characters.
      if (c == 0x20 || c == 0x21 || (c >= 0x23 && c <= 0x2E) ||
          (c >= 0x30 && c <= 0x5B) || (c >= 0x5D && c <= 0xFF))
        result += *b;
      else if (*b == Ch('\b'))
        result += Ch('\\'), result += Ch('b');
      else if (*b == Ch('\f'))
        result += Ch('\\'), result += Ch('f');
      else if (*b == Ch('\n'))
        result += Ch('\\'), result += Ch('n');
      else if (*b == Ch('\r'))
        result += Ch('\\'), result += Ch('r');
      else if (*b == Ch('\t'))
        result += Ch('\\'), result += Ch('t');
      else if (*b == Ch('/'))
        result += Ch('\\'), result += Ch('/');
      else if (*b == Ch('"') && b != beg && std::next(b) != e)
      {
        result += Ch('\\'), result += Ch('"');
      }
      else if (*b == Ch('"'))
        result += Ch('"');
      else if (*b == Ch('\\'))
        result += Ch('\\'), result += Ch('\\');
      else
      {
        const char *hexdigits = "0123456789ABCDEF";
        unsigned long u = (std::min)(static_cast<unsigned long>(
                                         static_cast<UCh>(*b)),
                                     0xFFFFul);
        unsigned long d1 = u / 4096;
        u -= d1 * 4096;
        unsigned long d2 = u / 256;
        u -= d2 * 256;
        unsigned long d3 = u / 16;
        u -= d3 * 16;
        unsigned long d4 = u;
        result += Ch('\\');
        result += Ch('u');
        result += Ch(hexdigits[d1]);
        result += Ch(hexdigits[d2]);
        result += Ch(hexdigits[d3]);
        result += Ch(hexdigits[d4]);
      }
      ++b;
    }
    return result;
  }

  template <class Ptree>
  void write_json_helper(std::basic_ostream<typename Ptree::key_type::value_type> &stream,
                         const Ptree &pt,
                         int indent, bool pretty)
  {

    typedef typename Ptree::key_type::value_type Ch;
    typedef typename std::basic_string<Ch> Str;

    // Value or object or array
    if (indent > 0 && pt.empty())
    {
      // Write value
      Str data = create_escapes(pt.template get_value<Str>());
      // stream << Ch('"') << data << Ch('"');
      stream << data;
    }
    else if (indent > 0 && pt.count(Str()) == pt.size())
    {
      // Write array
      stream << Ch('[');
      if (pretty)
        stream << Ch('\n');
      typename Ptree::const_iterator it = pt.begin();
      for (; it != pt.end(); ++it)
      {
        if (pretty)
          stream << Str(4 * (indent + 1), Ch(' '));
        write_json_helper(stream, it->second, indent + 1, pretty);
        if (boost::next(it) != pt.end())
          stream << Ch(',');
        if (pretty)
          stream << Ch('\n');
      }
      if (pretty)
        stream << Str(4 * indent, Ch(' '));
      stream << Ch(']');
    }
    else
    {
      // Write object
      stream << Ch('{');
      if (pretty)
        stream << Ch('\n');
      typename Ptree::const_iterator it = pt.begin();
      for (; it != pt.end(); ++it)
      {
        if (pretty)
          stream << Str(4 * (indent + 1), Ch(' '));
        // stream << Ch('"') << create_escapes(it->first) << Ch('"') << Ch(':');
        stream << create_escapes(it->first) << Ch(':');
        if (pretty)
          stream << Ch(' ');
        write_json_helper(stream, it->second, indent + 1, pretty);
        if (boost::next(it) != pt.end())
          stream << Ch(',');
        if (pretty)
          stream << Ch('\n');
      }
      if (pretty)
        stream << Str(4 * indent, Ch(' '));
      stream << Ch('}');
    }
  }

  void write_json(std::ostream &out, const boost::property_tree::ptree &pt)
  {
    if (!verify_json(pt, 0))
    {
      return;
    }
    write_json_helper(out, pt, 0, true);
    out << std::endl;
    // if (!out.good()) {

    // }
  }

  // template <typename T>
  // struct my_string_translator
  // {
  //   typedef T internal_type;
  //   typedef T external_type;

  //   boost::optional<T> get_value(const T &v) { return v.substr(1, v.size() - 2); }
  //   boost::optional<T> put_value(const T &v) { return '"' + v + '"'; }
  // };

} // namespace details
int main()
{
  boost::property_tree::ptree pt;
  boost::property_tree::read_json("test.json", pt);
  std::ostringstream oss;
  details::write_json(oss, pt);
  std::cout << oss.str();
  return 0;
}