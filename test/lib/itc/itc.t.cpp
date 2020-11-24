#include "lib/itc/itc.hpp"
#include <bitset>
#include "testing.t.hpp"

using namespace otto::itc;

// Tests
TEST_CASE (doctest::test_suite("itc") * "Basic Channel/Consumer/Producer linking and lifetime") {
  struct State {
    int i = 0;
    bool operator==(const State&) const = default;
  };

  ImmediateExecutor ex;

  SUBCASE ("Constructing consumer with channel registers it") {
    Channel<State> ch;
    Consumer<State> c1 = {ch, ex};

    REQUIRE(ch.consumers().size() == 1);
    REQUIRE(ch.consumers()[0] == &c1);

    Consumer<State> c2 = {ch, ex};

    REQUIRE(ch.consumers().size() == 2);
    REQUIRE(ch.consumers()[1] == &c2);
  }

  SUBCASE ("Constructing a producer with a channel registers it") {
    Channel<State> ch1;
    Producer<State> p = {ch1};

    REQUIRE(p.channels().size() == 1);
    REQUIRE(p.channels()[0] == &ch1);
    SUBCASE ("A producer can be registered to more channels with ch.set_producer") {
      Channel<State> ch2;
      ch2.set_producer(p);
      REQUIRE(p.channels().size() == 2);
      REQUIRE(p.channels()[1] == &ch2);
    }
  }

  SUBCASE ("Channel has a reference to its producer") {
    Channel<State> ch;
    REQUIRE(ch.producer() == nullptr);
    Producer<State> p = {ch};
    REQUIRE(ch.producer() == &p);

    SUBCASE ("Also when registered with set_producer") {
      Channel<State> ch2;
      ch2.set_producer(p);
      REQUIRE(ch2.producer() == &p);
      REQUIRE(ch.producer() == &p);
    }
  }

  SUBCASE ("Consumer has a reference to its channel") {
    Channel<State> ch;
    Consumer<State> c = {ch, ex};
    REQUIRE(c.channel() == &ch);
  }

  SUBCASE ("Bidirectional lifetime management") {
    SUBCASE ("Producer / Channel") {
      SUBCASE ("Producer destroyed before channel") {
        Channel<State> ch;
        {
          Producer<State> p = {ch};
        }
        REQUIRE(ch.producer() == nullptr);
      }
      SUBCASE ("Channel destroyed before producer") {
        auto ch = std::make_unique<Channel<State>>();
        Producer<State> p = {*ch};
        ch.reset();
        REQUIRE(p.channels().empty());
      }
    }
    SUBCASE ("Consumer / Channel") {
      SUBCASE ("Consumer destroyed before channel") {
        Channel<State> ch;
        {
          Consumer<State> p = {ch, ex};
        }
        REQUIRE(ch.consumers().empty());
      }
      SUBCASE ("Channel destroyed before consumer") {
        auto ch = std::make_unique<Channel<State>>();
        Consumer<State> c = {*ch, ex};
        ch.reset();
        REQUIRE(c.channel() == nullptr);
      }
    }
  }
}

TEST_CASE (doctest::test_suite("itc") * "Basic state passing") {
  ImmediateExecutor ex;
  struct S {
    int i = 0;
    bool operator==(const S&) const = default;
  };
  Channel<S> ch;
  Producer<S> p = {ch};
  struct C1 : Consumer<S> {
    using Consumer<S>::Consumer;

    void on_state_change(const S&) noexcept override
    {
      new_state_called++;
    }

    int new_state_called = 0;
  } c1 = {ch, ex};

  SUBCASE ("Access default state in Consumer") {
    REQUIRE(c1.state().i == 0);
  }

  struct P1 : Producer<S> {
  } p1 = {ch};

  SUBCASE ("Publish new state from producer") {
    p1.state().i = 1;
    REQUIRE(p1.state().i == 1);
    REQUIRE(c1.state().i == 0);
    p1.commit();
    REQUIRE(c1.state().i == 1);
    REQUIRE(c1.new_state_called == 1);
    p1.state().i = 2;
    p1.commit();
    REQUIRE(c1.state().i == 2);
    REQUIRE(c1.new_state_called == 2);
  }
}

TEST_CASE (doctest::test_suite("itc") * "prod/cons/chan of multiple states") {
  struct S1 {
    int i1 = 1;
    bool operator==(const S1&) const = default;
  };

  struct S2 {
    int i2 = 2;
    bool operator==(const S2&) const = default;
  };

  struct S3 {
    int i3 = 3;
    bool operator==(const S3&) const = default;
  };

  using Ch12 = Channel<S1, S2>;
  using C12 = Consumer<S1, S2>;
  using P12 = Producer<S1, S2>;

  static_assert(std::is_base_of_v<Channel<S1>, Ch12>);
  static_assert(std::is_base_of_v<Channel<S2>, Ch12>);
  static_assert(std::is_base_of_v<Consumer<S1>, C12>);
  static_assert(std::is_base_of_v<Consumer<S2>, C12>);
  static_assert(std::is_base_of_v<Producer<S1>, P12>);
  static_assert(std::is_base_of_v<Producer<S2>, P12>);

  //  using Ch123 = Channel<S1, meta::list<S2, S3>>;
  //  using C123 = Consumer<S1, meta::list<S2, S3>>;
  //  using P123 = Producer<S1, meta::list<S2, S3>>;
  //
  //  static_assert(std::is_base_of_v<Channel<S1>, Ch123>);
  //  static_assert(std::is_base_of_v<Channel<S2>, Ch123>);
  //  static_assert(std::is_base_of_v<Channel<S3>, Ch123>);
  //  static_assert(std::is_base_of_v<Consumer<S1>, C123>);
  //  static_assert(std::is_base_of_v<Consumer<S2>, C123>);
  //  static_assert(std::is_base_of_v<Consumer<S3>, C123>);
  //  static_assert(std::is_base_of_v<Producer<S1>, P123>);
  //  static_assert(std::is_base_of_v<Producer<S2>, P123>);
  //  static_assert(std::is_base_of_v<Producer<S3>, P123>);

  ImmediateExecutor ex;
  Ch12 ch;

  struct C1 : Consumer<S1, S2> {
    // Inherit the constructor
    using Consumer<S1, S2>::Consumer;

    void check_i1(int i)
    {
      REQUIRE(state<S1>().i1 == i);
    }

    void check_i2(int i)
    {
      REQUIRE(state<S2>().i2 == i);
    }

    void on_state_change(const S1& s) noexcept override
    {
      new_state1_called++;
    }

    void on_state_change(const S2& s) noexcept override
    {
      new_state2_called++;
    }

    int new_state1_called = 0;
    int new_state2_called = 0;
  } c1 = {ch, ex};

  SUBCASE ("Access default state in Consumer") {
    c1.check_i1(1);
    c1.check_i2(2);
  }

  struct P1 : Producer<S1, S2> {
    void test_produce1(int i)
    {
      state<S1>().i1 = i;
      commit<S1>();
    }
    void test_produce2(int i)
    {
      state<S2>().i2 = i;
      commit<S2>();
    }
  } p1 = {ch};

  SUBCASE ("Publish new state from producer") {
    p1.test_produce1(10);
    REQUIRE(c1.state<S1>().i1 == 10);
    REQUIRE(c1.new_state1_called == 1);
    REQUIRE(c1.state<S2>().i2 == 2);
    REQUIRE(c1.new_state2_called == 0);

    p1.test_produce2(20);
    REQUIRE(c1.state<S1>().i1 == 10);
    REQUIRE(c1.new_state1_called == 1);
    REQUIRE(c1.state<S2>().i2 == 20);
    REQUIRE(c1.new_state2_called == 1);
  }
}
