#include <type_traits>
#include <tuple>
#include <QTimer>
#include <QCoreApplication>
namespace QtUtility
{
	template <typename T>
	struct lambda_traits : lambda_traits<decltype(&T::operator())> {};

	template <typename ClassType, typename ReturnType, typename... Args>
	struct lambda_traits<ReturnType(ClassType::*)(Args...) const>
	{
		static const std::size_t arity = sizeof...(Args);
	};
	template <typename ClassType, typename ReturnType, typename... Args>
	struct lambda_traits<ReturnType(ClassType::*)(Args...)>
	{
		static const std::size_t arity = sizeof...(Args);
	};

	template<typename Func, std::size_t... Is, typename... Args>
	void static call_slot(Func slot, std::index_sequence<Is...>, Args&&... args) {
		slot(std::get<Is>(std::forward_as_tuple(std::forward<Args>(args)...))...);
	}
	template<typename ClassType, typename Func, std::size_t... Is, typename... Args>
	void static call_slot(ClassType* instance, Func slot, std::index_sequence<Is...>, Args&&... args) {
		(instance->*slot)(std::get<Is>(std::forward_as_tuple(std::forward<Args>(args)...))...);
	}
	class MyTimer : public QTimer
	{
	public:
		using QTimer::QTimer;
		std::function<void()> func;
	};
}
class Debounce : public QObject
{
	virtual bool isDebounce()
	{
		return true;
	}
public:
	int interval = 200;
	Debounce(int v, QObject* parent = NULL)
	{
		interval = v;
		setParent(parent ? parent : QCoreApplication::instance());
	}
	template <typename Func1, typename Func2>
	inline QMetaObject::Connection
		connect(typename QtPrivate::FunctionPointer<Func1>::Object* sender, Func1 signal,
			const QObject* context, Func2 slot, Qt::ConnectionType type = Qt::AutoConnection)
	{
		using namespace QtUtility;
		auto timer = new MyTimer(sender);
		timer->setSingleShot(true);
		QObject::connect(sender, signal, timer, [=](auto&&... args) {timer->func =
			[=]() {call_slot(slot, std::make_index_sequence<lambda_traits<Func2>::arity>(), (args)...); };
		if (isDebounce() || !timer->isActive())
			timer->start(interval); });
		return QObject::connect(timer, &QTimer::timeout, context, [=]() {timer->func(); }, type);
	}
	template <typename Func1, typename Func2>
	inline QMetaObject::Connection
		connect(typename QtPrivate::FunctionPointer<Func1>::Object* sender, Func1 signal,
			typename QtPrivate::FunctionPointer<Func2>::Object* context, Func2 slot, Qt::ConnectionType type = Qt::AutoConnection)
	{
		using namespace QtUtility;
		auto timer = new MyTimer(sender);
		timer->setSingleShot(true);
		QObject::connect(sender, signal, timer, [=](auto&&... args) {timer->func =
			[=]() {call_slot(context, slot, std::make_index_sequence<lambda_traits<Func2>::arity>(), (args)...); };
		if (isDebounce() || !timer->isActive())
			timer->start(interval); });
		return QObject::connect(timer, &QTimer::timeout, context, [=]() {timer->func(); }, type);
	}
};
class Throttle : public Debounce
{
	virtual bool isDebounce()
	{
		return false;
	}
public:
	using Debounce::Debounce;
};
#include <QApplication>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QWheelEvent>
#include <QMainWindow>
class PrintClass :public QObject
{
public:
	void print(int v)
	{
		printf("Throttle PrintClass  %d  \n", v);
	}
};
int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	auto w = new QWidget();
	auto sl = new QSlider(Qt::Horizontal, w);
	auto dsp = new QDoubleSpinBox(w);
	w->show();
	sl->move(0, 30);
	w->resize(200, 100);
	PrintClass pt;
	{
		auto thr = new Throttle(500);
		auto deb = new Debounce(300);
		QObject::connect(dsp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), dsp, [=](double v) {
			printf("QObject send \n ");
			emit sl->rangeChanged(0, v); });
		thr->connect(sl, &QSlider::rangeChanged, sl, [](int v0, int v1) {printf("Throttle   %d  %d\n", v0, v1); });
		deb->connect(dsp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), dsp, [=](double v) {
			printf("Debounce QDoubleSpinBox %f  \n", v); });
		deb->connect(sl, &QSlider::valueChanged, sl, []() {printf("Debounce QSlider1 \n "); });
		thr->connect(sl, &QSlider::valueChanged, &pt, &PrintClass::print);
	}
	return app.exec();
}