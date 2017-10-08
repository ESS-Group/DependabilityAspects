/* 
 * This file is part of the library of dependability aspects.
 * See: http://dx.doi.org/10.17877/DE290R-17995
 * Copyright (c) 2017 Christoph Borchert.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CHECKSUMMING_TYPETRAITS_H__
#define __CHECKSUMMING_TYPETRAITS_H__

namespace CoolChecksum {

//XXX: see
// http://www.boost.org/doc/libs/1_32_0/libs/type_traits/cxx_type_traits.htm
// http://stackoverflow.com/questions/6128715/boost-type-traits-is-array
// http://www.boost.org/doc/libs/1_48_0/boost/type_traits/


//TODO: what to do about unions? (they are currently ignored (hence treated as classes))

//test for classes, structs
template<typename T> int is_class_tester(...);
template<typename T> char is_class_tester(void(T::*)(void));
template<typename T> struct __isClass {
  enum { RET=((sizeof(is_class_tester<T>(0))==1)?1:0) };
};

/*
template <class T> struct wrap {};

//                   retval  rekursive fct-ptr                args
template<typename T> T       (* is_array_tester1(wrap<T>) ) (wrap<T>);
char is_array_tester1(...);

//                                      retval fct-ptr  args
template<typename T> int is_array_tester2(T    (*)    (wrap<T>) ); //no
char is_array_tester2(...); //yes

template<typename T>
struct __isArray
{
  enum { RET=(sizeof(is_array_tester2(is_array_tester1(wrap<T>())))==1)?1:0 };
};
*/
//---------------------------------
template<class T>
struct __isArray{
  static const bool value = false;
};

template<class T, __SIZE_TYPE__ N> //TODO: std::size_t ?
struct __isArray<T[N]>{
  static const bool value = true;
};


template<class T>
struct __isUnsizedArray{
  static const bool value = false;
};

template<class T>
struct __isUnsizedArray<T[]>{
  static const bool value = true;
};
//---------------------------------

template <typename T> 
struct remove_bounds
{ typedef T type; };

template <typename T, __SIZE_TYPE__ N> //TODO: std::size_t ?
struct remove_bounds<T[N]>
{ typedef T type; };

//---------------------------------

//recursion ...
template<typename T, bool is_array = (__isArray<T>::value)>
struct getArrayBaseType {
  typedef typename getArrayBaseType<typename remove_bounds<T>::type>::Type Type; 
};
template<typename T>
struct getArrayBaseType<T,false> {
  typedef T Type; 
};


//----

// This is the one to go ;-)
template<typename T, bool is_class = (__isClass<T>::RET)>
struct FoldArray {
  enum { isClass = __isClass<T>::RET };
};
template<typename T>
struct FoldArray<T,false> {
  enum { isClass = __isClass<typename getArrayBaseType<T>::Type>::RET };
};

// --- test for constness ---
template <typename T>
struct is_const {
    enum { IS_CONST = 0 };
};
template <typename T>
struct is_const<const T> {
    enum { IS_CONST = 1 };
};

//-----------------------------------------------------------------------------
//--------------------- TEST FOR BASE AND DERIVED CLASSES ---------------------
//-----------------------------------------------------------------------------
template <typename B, typename D>
struct bd_helper
{
    template <typename T>
    static char check_sig(D const volatile *, T);
    static int  check_sig(B const volatile *, int);
    //static char check_sig(D const volatile *, long); //VC++ workaround
    //static int  check_sig(B const volatile * const&, int); //VC++ workaround
};

template<typename B, typename D>
struct is_base_and_derived
{
    struct Host
    {
        operator B const volatile *() const;
        //operator B const volatile * const&() const; //VC++ workaround
        operator D const volatile *();
    };

    enum { RET = sizeof(bd_helper<B,D>::check_sig(Host(), 0)) == sizeof(char) };
};

} //CoolChecksum

#endif // __CHECKSUMMING_TYPETRAITS_H__

