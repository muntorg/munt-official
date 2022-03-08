import store from "./store";

export function formatMoneyForDisplay(monetaryAmount, isFiat) {
  //fixme: This should truncate not round
  let decimalPlaces = 2;

  if (isFiat) {
    decimalPlaces = 2;
  } else {
    decimalPlaces = store.state.app.decimals;
  }

  return (monetaryAmount / 100000000).toFixed(decimalPlaces);
}

export function displayToMonetary(displayAmount) {
  return displayAmount * 100000000;
}
