#ifndef INCLUDE_FIRESTARTER_ENVIRONMENT_X86_PAYLOAD_AVX512PAYLOAD_H
#define INCLUDE_FIRESTARTER_ENVIRONMENT_X86_PAYLOAD_AVX512PAYLOAD_H

#include <firestarter/Environment/X86/Payload/X86Payload.hpp>

namespace firestarter::environment::x86::payload {
	class AVX512Payload : public X86Payload {
		public:
			AVX512Payload(llvm::StringMap<bool> *supportedFeatures) : X86Payload(supportedFeatures, {"avx512f"}, "AVX512") {};

			void compilePayload(llvm::StringMap<unsigned> proportion) override;
			std::list<std::string> getAvailableInstructions(void) override;
			void init(...) override;
			void highLoadFunction(...) override;
	};
}

#endif