package object

func NewEnvironment() *Environment {
	s := make(map[string]Object)
	return &Environment{store: s}
}

func NewEnclosedEnvironment(env *Environment) *Environment {
	enclosedEnv := NewEnvironment()
	enclosedEnv.outer = env
	return enclosedEnv
}

type Environment struct {
	store map[string]Object
	outer *Environment
}

func (e *Environment) Get(name string) (Object, bool) {
	if obj, ok := e.store[name]; ok {
		return obj, true
	}
	if e.outer != nil {
		return e.outer.Get(name)
	}
	return nil, false
}

func (e *Environment) Set(name string, val Object) Object {
	e.store[name] = val
	return val
}
