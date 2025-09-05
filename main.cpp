
#include <algorithm>
#include <cerrno>
#include <charconv> // std::from_chars
#include <cstdlib> // EXIT_SUCCESS, EXIT_FAILURE
#include <cstdint> // std::size_t
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error> // std::errc

static_assert(sizeof(off_t) <= sizeof(std::size_t), "invalid size of off_t");

extern "C"
{
	#include <fcntl.h> // ::open()
	#include <unistd.h> // ::close()

	#include <sys/mman.h> // ::mmap()
}

void
help(
)
{
	std::cout
		<< "./a.out path-to-hugetlb-file [mmap-size] [mmap-offset]" << std::endl;
}

struct
raw_fd
{
	raw_fd(
		int fd,
		const std::string_view name
	)
	: fd_{fd}
	{
		if (-1 == fd_)
		{
			throw std::system_error{errno, std::system_category(), std::string{name}};
		}
	}

	raw_fd(
		const std::filesystem::path& pathname
	)
	: raw_fd{::open(pathname.c_str(), O_LARGEFILE, O_RDONLY), "open()"}
	{
	}

	int
	get(
	) const noexcept
	{
		return fd_;
	}

	int
	operator*(
	) const noexcept
	{
		return get();
	}

	~raw_fd(
	) noexcept
	{
		if (-1 != fd_)
		{
			::close(fd_);
		}
	}

private:
	int fd_ = -1;
};


// you could also use std::unique_ptr with a custom deleter
struct
mmap_ptr
{
	mmap_ptr(
		void* ptr,
		std::size_t sz
	)
	: ptr_{ptr}
	, sz_{sz}
	{
		if (MAP_FAILED == ptr_)
		{
			throw std::system_error{errno, std::system_category(), "mmap()"};
		}
	}

	mmap_ptr(
		const raw_fd& fd,
		std::size_t sz,
		std::size_t offset
	)
	: mmap_ptr{
		::mmap(
			nullptr,
			sz,
			PROT_READ,
			MAP_PRIVATE | MAP_HUGE_1GB,
			*fd,
			static_cast<off_t>(offset)
		),
		sz
	}
	{
	}

	void*
	get(
	) const noexcept
	{
		return ptr_;
	}

	void*
	operator*(
	) const noexcept
	{
		return get();
	}

	~mmap_ptr(
	) noexcept
	{
		if (ptr_)
		{
			::munmap(ptr_, sz_);
		}
	}

private:
	void* ptr_ = nullptr;
	std::size_t sz_ = 0;
};

int
main(
	int argc,
	char **argv
)
try
{
	if (argc < 2)
	{
		help();
		return EXIT_FAILURE;
	}
	std::span<char*> args{argv, static_cast<std::size_t>(argc)};
	for (const auto& arg : args)
	{
		std::cout << arg << ' ';
	}
	std::cout << std::endl;

	std::string_view program{args[0]};
	std::filesystem::path pathname{args[1]};

	const std::size_t file_sz{std::filesystem::file_size(pathname)};
	if (file_sz <= 0)
	{
		throw std::runtime_error{std::string{pathname} + " is empty and cannot be memory-mapped!"};
	}
	constexpr std::size_t OneGiB = (1024 * 1024 * 1024);
	if (file_sz < OneGiB)
	{
		throw std::runtime_error{"file is not large enough to use 1GB huge tlb!"};
	}

	std::string_view mmap_size_str{2 < args.size() ? args[2] : "0"};
	std::string_view mmap_offset_str{3 < args.size() ? args[3] : "0"};
	std::cout
		<< "mmap_size_str: " << mmap_size_str << ", "
		<< "mmap_offset_str: " << mmap_offset_str << std::endl;

	auto parse_size = [&](
		const std::string_view v,
		const std::string_view name,
		bool auto_sz
	)
	{
		std::size_t sv{0};
		auto r = std::from_chars(begin(v), end(v), sv);
		if (std::errc{} != r.ec)
		{
			throw std::runtime_error{std::string{"invalid mmap size: "} + std::string{v}};
		}
		if ((auto_sz) && (0 == sv))
		{
			sv = file_sz;
		}
		if (auto_sz)
		{
			sv -= (sv % OneGiB);
		}
		return sv;
	};

	std::size_t mmap_size{parse_size(mmap_size_str, "mmap-size", true)};
	std::size_t mmap_offset{parse_size(mmap_offset_str, "mmap-offset", false)};
	std::cout << "size: " << mmap_size << ", offset: " << mmap_offset << std::endl;

	raw_fd fd{pathname};
	std::cout << "opened file " << *fd << std::endl;

	mmap_ptr ptr{pathname, mmap_size, mmap_offset};
	std::cout << "memory-mapped to 0x" << std::hex << std::setw(16) << std::setfill('0') << *ptr << std::endl;

	std::span bytes{reinterpret_cast<std::uint8_t*>(*ptr), mmap_size};
	std::size_t number_of_zeroes{0};
	number_of_zeroes += std::count(begin(bytes), end(bytes), 0);
	std::cout << "number of zeroes: " << std::dec << number_of_zeroes << std::endl;

	return EXIT_SUCCESS;
}
catch (const std::system_error& e)
{
	std::cerr
		<< "error calling " << e.what() << "(...):\n"
		<< e.code() << ": " << e.code().message() << std::endl;
	return EXIT_FAILURE;
}
catch (const std::exception& e)
{
	std::cerr << "unhandled exception:\n" << e.what() << std::endl;
	return EXIT_FAILURE;
}
catch (...)
{
	std::cerr << "unhandled exception of unknown type" << std::endl;
	return EXIT_FAILURE;
}
