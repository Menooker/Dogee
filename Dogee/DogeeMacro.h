#ifndef __DOGEE_MACRO_H_ 
#define __DOGEE_MACRO_H_ 

#define self Dogee::ReferenceObject(this)
#define DefBegin(Parent) static const int _BASE_ = __COUNTER__;static const int _PBASE_ = Parent::_LAST_;
#define Def(Type,Name) Dogee::Value<Type,__COUNTER__- _BASE_ - 1 + _PBASE_> Name
#define DefRef(Type,isVirtual,Name) Dogee::Value<Dogee::Ref<Type,isVirtual>,__COUNTER__- _BASE_ - 1 + _PBASE_> Name

//DefEnd must be declared public
#define DefEnd() static const int _LAST_ = __COUNTER__- _BASE_ - 1 + _PBASE_ ;static const int CLASS_ID = __COUNTER__;
#define RegVirt(CurrentClass) static Dogee::AutoRegisterObject<CurrentClass> __REG_OBJECT__##CurrentClass##__;

#endif


