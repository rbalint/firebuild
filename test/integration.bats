#!/usr/bin/env bats

@test "bash -c ls" {
      result=$(./run-firebuild -- bash -c "ls integration.bats")
      [ "$result" = "integration.bats" ]
}

@test "bash exec chain" {
      result=$(./run-firebuild -- bash -c exec\ bash\ -c\ exec\\\ bash\\\ -c\\\ ls\\\\\\\ integration.bats)
      [ "$result" = "integration.bats" ]
}

@test "simple pipe" {
      result=$(./run-firebuild -- bash -c 'seq 10000 | grep ^9')
      [ "$result" = "$(seq 10000 | grep ^9)" ]
}

@test "2k parallel echo-s" {
      result="$(./run-firebuild -- parallel -j 2000 echo -- $(seq 1 2000))"
      [ "$(echo "$result" | sort)" = "$(seq 1 2000 | sort)" ]
}

@test "system()" {
      result=$(./run-firebuild -- ./test_system)
      [ "$result" = "ok" ]
}