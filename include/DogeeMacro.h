#ifndef __DOGEE_MACRO_H_ 
#define __DOGEE_MACRO_H_ 

#define self Dogee::ReferenceObject(this)
#define DefBegin(Parent) static const int _BASE_ = __COUNTER__;static const int _PBASE_ = Parent::_LAST_;
//#define Def(Type,Name) Dogee::Value<Type,2*(__COUNTER__- _BASE_ - 1 + _PBASE_)> Name
#define Def(Name,...) Dogee::Value<__VA_ARGS__,2*(__COUNTER__- _BASE_ - 1 + _PBASE_)> Name
#define DefRef(Type,isVirtual,Name) Dogee::Value<Dogee::Ref<Type,isVirtual>,2*(__COUNTER__- _BASE_ - 1 + _PBASE_)> Name

//DefEnd must be declared public
#define DefEnd() static const int _LAST_ = __COUNTER__- _BASE_ - 1 + _PBASE_ ;

#define AutoRegisterObjectName(n) __REG_OBJECT__##n##__
#define _AutoRegisterObjectName(n) AutoRegisterObjectName(n)
#define RegVirt(...)  static Dogee::AutoRegisterObject<__VA_ARGS__> _AutoRegisterObjectName(__COUNTER__);


//#define DefGlobal(Type,Name) Dogee::ArrayElement<Type> Name(0,Dogee::RegisterGlobalVariable());
//#define ExternGlobal(Type,Name) extern Dogee::ArrayElement<Type> Name;
#define DefGlobal(Name,...) Dogee::ArrayElement<__VA_ARGS__> Name(0,Dogee::RegisterGlobalVariable());
#define ExternGlobal(Name,...) extern Dogee::ArrayElement<__VA_ARGS__> Name;


#define AutoRegisterFuncName(n) __REG_FUNC__##n##__
#define _AutoRegisterFuncName(n) AutoRegisterFuncName(n)
#define RegFunc(Func)  static Dogee::AutoRegisterThreadProcClass AutoRegisterFuncName(__COUNTER__)(Func);

#endif


