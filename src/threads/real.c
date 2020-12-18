#include "real.h"
struct real int_to_real (int n)
{
  struct real structreal;
  structreal.val = n << FIXED_POINT;
  return structreal;
}
//convert fixed point x to integer (rounding towards zero)
int real_truncate (struct real x)
{
  return x.val >> FIXED_POINT;
}
//convert fixed point x to integer (rounding to nearest)
int real_round (struct real x)
{
  if (x.val >= 0)
    return (x.val + (1 << (FIXED_POINT - 1))) >> FIXED_POINT;
  else
    return (x.val - (1 << (FIXED_POINT - 1))) >> FIXED_POINT;
}
// returns real x +  real y
struct real add_real_real (struct real x, struct real y)
{
  struct real new_real;
  new_real.val = x.val + y.val;
  return new_real;
}
// returns real x - real y
struct real sub_real_real (struct real x, struct real y)
{
  struct real new_real;
  new_real.val = x.val - y.val;
  return new_real;
}
struct real add_real_int (struct real x, int64_t n)
{
  struct real new_real;
  new_real.val = x.val + int_to_real (n).val;
  return new_real;
}
struct real sub_real_int (struct real x, int64_t n)
{
  struct real new_real;
  new_real.val = x.val - int_to_real (n).val;
  return new_real;
}
struct real mul_real_real (struct real x, struct real y)
{
  struct real new_real;
  new_real.val = (((int64_t) x.val) * y.val) >> FIXED_POINT;
  return new_real;
}
struct real mul_real_int (struct real x, int64_t n)
{
  x.val *= n;
  return x;
}
struct real div_real_real (struct real x, struct real y)
{
  struct real new_real;
  new_real.val = (((int64_t) x.val) << FIXED_POINT) / y.val;
  return new_real;
}
struct real div_real_int (struct real x, int64_t n)
{
  x.val /= n;
  return x;
}
