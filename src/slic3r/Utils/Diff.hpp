#include <string>
#include <vector>

namespace slic3r {
	struct EditScriptAction {
		enum class ActionType {
			keep, //from A
			remove, //from A
			insert //from B
		};

		ActionType action;
		size_t offset;
		size_t count;

		EditScriptAction(ActionType _action, size_t _off, size_t _count) :
			action(_action),
			offset(_off),
			count(_count)
		{};
	};

	class Diff {
	public:
		Diff(std::string _str1, std::string _str2);

		const std::vector<EditScriptAction>& getSolution();
#ifdef _DEBUG
		void selfTest(int count = 100);
#endif
	private:
		std::vector<EditScriptAction> solution;

		void solve(std::string str1, std::string str2);
	};
}