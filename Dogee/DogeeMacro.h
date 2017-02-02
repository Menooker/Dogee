
#define self Dogee::ReferenceObject(this)
#define DefBegin(Parent) static const int _BASE_ = __COUNTER__;static const int _PBASE_ = Parent::_LAST_;
#define Def(Type,Name) Dogee::Value<Type,__COUNTER__- _BASE_ - 1 + _PBASE_> Name
#define DefEnd() static const int _LAST_ = __COUNTER__- _BASE_ - 1 + _PBASE_ ;