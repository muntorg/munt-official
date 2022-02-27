export function formatMoneyForDisplay(monetaryAmount) {
  //fixme: This should truncate not round
  return (monetaryAmount / 100000000).toFixed(2);
}

export function displayToMonetary(displayAmount) {
  return displayAmount * 100000000;
}
