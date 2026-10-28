#include <gtest/gtest.h>
#include <string>
#include "core/log_ring.h"

/*
Журнал портала. Пользователь скачивает его, когда Ватериус не подключается к
роутеру, поэтому важен хвост: последняя попытка подключения обязана уцелеть,
сколько бы ни было написано до неё.
*/

namespace
{
    void write(LogRing &ring, const std::string &text)
    {
        log_ring_write(ring, (const uint8_t *)text.data(), text.size());
    }

    // Так читает портал: позиция и конец снимаются один раз, дальше кусками
    std::string read_all(const LogRing &ring, uint32_t &pos, uint32_t end, size_t chunk)
    {
        std::string out;
        uint8_t buf[64];
        size_t n;
        while ((n = log_ring_read(ring, pos, end, buf, chunk)) > 0)
            out.append((const char *)buf, n);
        return out;
    }

    std::string snapshot(const LogRing &ring, size_t chunk = 64)
    {
        uint32_t pos = log_ring_oldest(ring);
        return read_all(ring, pos, ring.written, chunk);
    }
}

TEST(LogRing, NoBufferMeansNoJournal)
{
    // Память под журнал не выделилась - лог идёт только в UART, отдавать нечего
    LogRing ring;

    write(ring, "WIFI: Connecting...");

    EXPECT_EQ(ring.written, 0u);
    EXPECT_EQ(snapshot(ring), "");
}

TEST(LogRing, ShortLogComesBackWhole)
{
    char mem[16];
    LogRing ring{mem, sizeof(mem)};

    write(ring, "abc");
    write(ring, "def");

    EXPECT_EQ(snapshot(ring), "abcdef");
}

TEST(LogRing, OverflowKeepsTheTail)
{
    char mem[8];
    LogRing ring{mem, sizeof(mem)};

    write(ring, "0123456789");

    EXPECT_EQ(log_ring_oldest(ring), 2u);
    EXPECT_EQ(snapshot(ring), "23456789");
}

TEST(LogRing, ChunksAcrossTheWrapJoinInOrder)
{
    // Сервер отдаёт кусками по размеру окна TCP, граница куска - где придётся
    char mem[8];
    LogRing ring{mem, sizeof(mem)};

    write(ring, "0123456789abc");

    EXPECT_EQ(snapshot(ring, 3), "56789abc");
}

TEST(LogRing, LinesWrittenDuringDownloadAreNotSent)
{
    // Обработчик запроса сам пишет в лог. Отдаётся то, что было на момент запроса
    char mem[16];
    LogRing ring{mem, sizeof(mem)};
    write(ring, "before");

    uint32_t pos = log_ring_oldest(ring);
    const uint32_t end = ring.written;
    write(ring, "after");

    EXPECT_EQ(read_all(ring, pos, end, 4), "before");
}

TEST(LogRing, OverwrittenPartIsSkippedNotMixed)
{
    // Затёртое посреди отдачи пропадает, но в файл не попадает новый текст
    // на месте старого
    char mem[8];
    LogRing ring{mem, sizeof(mem)};
    write(ring, "ABCDEFGH");

    uint32_t pos = log_ring_oldest(ring);
    const uint32_t end = ring.written;
    uint8_t buf[2];
    ASSERT_EQ(log_ring_read(ring, pos, end, buf, sizeof(buf)), 2u);

    write(ring, "123456");

    EXPECT_EQ(read_all(ring, pos, end, 4), "GH");
}
